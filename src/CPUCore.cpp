#include "../include/CPUCore.h"
#include <iomanip>

void CPUCore::init(const std::string& program_file)
{
    pipe_regs.reset();
    reg_file = RegisterFile();

    memory.load_program(program_file);

    pc = config.pc_init;
    cycle_count = 0;
    instr_count = 0;
    stats.reset();
    running = true;

    // 初始化栈指针 (x2)，指向 64KB 内存顶部附近
    reg_file.write_rd(2, config.sp_init, true);

    // 初始化陷阱向量基址 (mtvec)，用于异常/中断处理
    reg_file.write_csr(CSR_MTVEC, config.mtvec_init);
}

void CPUCore::run()
{
    std::cout << "--- CPU Simulation Started ---" << std::endl;
    while (running)
    {
        tick();
    }
    std::cout << "--- CPU Simulation Halted (EBREAK detected) ---" << std::endl;
    dumpState();
}

void CPUCore::tick()
{
    // 0. 更新定时器并检查待处理中断
    memory.increment_timer();

    // 更新 MTIP 硬件位（软件写入 CSR 时只读）
    reg_file.hw_set_mtip(memory.timer_interrupt_pending());

    checkAndHandleInterrupts();

    if (!running) return;

    // 流水线阶段按反向顺序执行（模拟硬件行为）
    writeBack();
    memoryAccess();
    execute();
    decodeAndIssue();
    fetch();

    cycle_count++;
}

// =========================================================================
// 1. 写回阶段 (WB)
// =========================================================================
void CPUCore::writeBack()
{
    for (int i = 0; i < 2; ++i)
    {
        auto& slot = pipe_regs.mem_wb.slots[i];
        if (slot.valid)
        {
            // EBREAK (0x00100073)：停止模拟器
            if (slot.instr == 0x00100073)
            {
                running = false;
                continue;
            }

            // 全零指令（未初始化内存）：触发致命错误
            if (slot.instr == 0x00000000)
            {
                std::cout << "\n[Fatal Error] Executed illegal instruction 0x00000000! "
                    << "Emergency halting to prevent Runaway PC." << std::endl;
                running = false;
                continue;
            }

            if (slot.regWrite && slot.rd != 0)
            {
                reg_file.write_rd(slot.rd, slot.result, true);
            }

            instr_count++;
        }
    }
}

// =========================================================================
// 2. 访存阶段 (MEM)
// =========================================================================
void CPUCore::memoryAccess()
{
    pipe_regs.mem_wb = MEM_WB_Buffer();

    for (int i = 0; i < 2; ++i)
    {
        if (!pipe_regs.ex_mem.slots[i].valid) continue;

        pipe_regs.mem_wb.slots[i] = pipe_regs.ex_mem.slots[i];

        Word funct3 = (pipe_regs.ex_mem.slots[i].instr >> 12) & 0x7;
        Addr addr = pipe_regs.ex_mem.mem_addr[i];

        // --- 加载操作 ---
        if (pipe_regs.ex_mem.memRead[i])
        {
            // 确定访问宽度
            Byte n = (funct3 == 0 || funct3 == 4) ? 1 : ((funct3 == 1 || funct3 == 5) ? 2 : 4);

            // 对齐检查
            if ((n == 4 && (addr & 0x3)) || (n == 2 && (addr & 0x1))) {
                std::cout << "[Trap] Load address misaligned: 0x" << std::hex << addr << std::dec << std::endl;
                Addr fault_pc = pipe_regs.ex_mem.pc + (i * 4);
                reg_file.write_csr(CSR_MEPC, fault_pc);
                reg_file.write_csr(CSR_MCAUSE, MCAUSE_LOAD_MISALIGNED);
                reg_file.write_csr(CSR_MTVAL, addr);
                pc = reg_file.read_csr(CSR_MTVEC);
                pipe_regs.flush();
                return;
            }

            // MMU：虚拟地址转物理地址
            Word satp_val = reg_file.read_csr(CSR_SATP);
            TranslationResult trans = memory.translate(addr, false, false, satp_val);
            if (trans.fault) {
                std::cout << "[Trap] Load page fault at VA=0x" << std::hex << addr << std::dec << std::endl;
                Addr fault_pc = pipe_regs.ex_mem.pc + (i * 4);
                handleTrap(fault_pc, trans.cause, addr);
                return;
            }
            Addr phys_addr = trans.paddr;

            Word raw_data = memory.read(phys_addr, n);

            // 符号扩展：LB、LH
            if (funct3 == 0)      // LB: 8-bit -> 32-bit signed
                raw_data = static_cast<Word>(static_cast<int32_t>(static_cast<int8_t>(raw_data & 0xFF)));
            else if (funct3 == 1) // LH: 16-bit -> 32-bit signed
                raw_data = static_cast<Word>(static_cast<int32_t>(static_cast<int16_t>(raw_data & 0xFFFF)));

            pipe_regs.mem_wb.slots[i].result = raw_data;
        }

        // --- 存储操作 ---
        if (pipe_regs.ex_mem.memWrite[i])
        {
            Byte n = (funct3 == 0) ? 1 : ((funct3 == 1) ? 2 : 4);

            // 对齐检查
            if ((n == 4 && (addr & 0x3)) || (n == 2 && (addr & 0x1))) {
                std::cout << "[Trap] Store address misaligned: 0x" << std::hex << addr << std::dec << std::endl;
                Addr fault_pc = pipe_regs.ex_mem.pc + (i * 4);
                reg_file.write_csr(CSR_MEPC, fault_pc);
                reg_file.write_csr(CSR_MCAUSE, MCAUSE_STORE_MISALIGNED);
                reg_file.write_csr(CSR_MTVAL, addr);
                pc = reg_file.read_csr(CSR_MTVEC);
                pipe_regs.flush();
                return;
            }

            // MMU：虚拟地址转物理地址
            Word satp_val = reg_file.read_csr(CSR_SATP);
            TranslationResult trans = memory.translate(addr, true, false, satp_val);
            if (trans.fault) {
                std::cout << "[Trap] Store page fault at VA=0x" << std::hex << addr << std::dec << std::endl;
                Addr fault_pc = pipe_regs.ex_mem.pc + (i * 4);
                handleTrap(fault_pc, trans.cause, addr);
                return;
            }
            Addr phys_addr = trans.paddr;

            memory.write(phys_addr, n, pipe_regs.ex_mem.mem_data[i]);
        }
    }
}

// =========================================================================
// 3. 执行阶段 (EX)
// =========================================================================
void CPUCore::execute()
{
    exec_engine.execute(pipe_regs.id_ex, pipe_regs.ex_mem, pipe_regs.mem_wb, reg_file);

    // 提交 CSR 写入（在分支/跳转处理之前）
    for (int i = 0; i < 2; ++i) {
        if (pipe_regs.ex_mem.slots[i].valid && pipe_regs.ex_mem.slots[i].d.is_csr) {
            reg_file.write_csr(pipe_regs.ex_mem.slots[i].d.csr_addr,
                               pipe_regs.ex_mem.slots[i].csr_write_val);
        }
    }

    // --- 控制冒险：分支/跳转 ---
    if (pipe_regs.ex_mem.slots[0].valid && pipe_regs.ex_mem.slots[0].d.is_branch)
    {
        stats.branch_flushes++;
        Addr target_pc = pipe_regs.ex_mem.slots[0].jump_target;
        pc = target_pc;

        // 冲刷前端阶段
        pipe_regs.if_id = IF_ID_Buffer();
        pipe_regs.id_ex = ID_EX_Buffer();

        // 无效化槽位 1（分支命中后的气泡）
        pipe_regs.ex_mem.slots[1] = PipelineSlot();
        pipe_regs.ex_mem.slots[1].valid = false;
        pipe_regs.ex_mem.memRead[1] = false;
        pipe_regs.ex_mem.memWrite[1] = false;
    }

    // --- 异常处理：检查两个槽位（先查槽位 0，更早的指令优先） ---
    // 槽位 0 检查
    if (pipe_regs.ex_mem.slots[0].valid) {
        auto& d = pipe_regs.ex_mem.slots[0].d;
        Addr current_instruction_pc = pipe_regs.ex_mem.pc;

        if (d.is_ecall) {
            std::cout << "[Trap] ECALL detected at 0x" << std::hex << current_instruction_pc << std::dec << std::endl;
            handleTrap(current_instruction_pc + 4, MCAUSE_ECALL_M, 0);
        }
        else if (d.is_illegal) {
            std::cout << "[Trap] Illegal instruction 0x" << std::hex
                      << pipe_regs.ex_mem.slots[0].instr
                      << " at PC 0x" << current_instruction_pc << std::dec << std::endl;
            handleTrap(current_instruction_pc, MCAUSE_ILLEGAL,
                       pipe_regs.ex_mem.slots[0].instr);
        }
        else if (d.is_mret) {
            std::cout << "[Trap] MRET detected. Returning..." << std::endl;
            // 从 MPIE 恢复 MIE，设置 MPIE 为 1（RISC-V 规范）
            Word mstatus_val = reg_file.read_csr(CSR_MSTATUS);
            Word mstatus_new = mstatus_val & ~MSTATUS_MIE;
            if (mstatus_val & MSTATUS_MPIE) {
                mstatus_new |= MSTATUS_MIE;   // MIE = MPIE
            }
            mstatus_new |= MSTATUS_MPIE;       // MPIE = 1
            reg_file.write_csr(CSR_MSTATUS, mstatus_new);

            pc = reg_file.read_csr(CSR_MEPC);
            pipe_regs.flush();
            pipe_regs.ex_mem.slots[0].valid = false;
            pipe_regs.ex_mem.slots[1].valid = false;
        }
        else if (pipe_regs.ex_mem.slots[1].valid) {
            // 槽位 0 正常，检查槽位 1
            auto& d1 = pipe_regs.ex_mem.slots[1].d;
            Addr slot1_pc = pipe_regs.ex_mem.pc + 4;  // 槽位 1 的 PC = 槽位 0 的 PC + 4

            if (d1.is_ecall) {
                std::cout << "[Trap] ECALL detected at 0x" << std::hex << slot1_pc << std::dec << std::endl;
                handleTrap(slot1_pc + 4, MCAUSE_ECALL_M, 0);
            }
            else if (d1.is_illegal) {
                std::cout << "[Trap] Illegal instruction 0x" << std::hex
                          << pipe_regs.ex_mem.slots[1].instr
                          << " at PC 0x" << slot1_pc << std::dec << std::endl;
                handleTrap(slot1_pc, MCAUSE_ILLEGAL,
                           pipe_regs.ex_mem.slots[1].instr);
            }
            else if (d1.is_mret) {
                std::cout << "[Trap] MRET detected in slot 1. Returning..." << std::endl;
                pc = reg_file.read_csr(CSR_MEPC);
                pipe_regs.flush();
                pipe_regs.ex_mem.slots[0].valid = false;
                pipe_regs.ex_mem.slots[1].valid = false;
            }
        }
    }
}

// =========================================================================
// 4. 译码发射阶段 (ID)
// =========================================================================
void CPUCore::decodeAndIssue()
{
    issue_unit.decodeAndIssue(pipe_regs.if_id, pipe_regs.id_ex, reg_file, pipe_regs, stats);

    // 统计发射情况
    if (pipe_regs.id_ex.slots[0].valid && pipe_regs.id_ex.slots[1].valid) {
        stats.dual_issues++;
    } else if (pipe_regs.id_ex.slots[0].valid) {
        stats.single_issues++;
    } else {
        stats.stall_cycles++;
    }
}

// =========================================================================
// 5. 取指阶段 (IF)
// =========================================================================
void CPUCore::fetch()
{
    // MMU 地址转换辅助函数（用于取指）
    auto fetch_instr = [&](Addr vaddr) -> Word {
        Word satp_val = reg_file.read_csr(CSR_SATP);
        TranslationResult trans = memory.translate(vaddr, false, true, satp_val);
        if (trans.fault) {
            std::cout << "[Trap] Instruction page fault at PC=0x" << std::hex << vaddr << std::dec << std::endl;
            handleTrap(vaddr, trans.cause, vaddr);
            return 0;
        }
        return memory.read(trans.paddr, 4);
    };

    // 情况A：IF_ID 槽位 0 仍有效（停顿），不取指
    if (pipe_regs.if_id.slots[0].valid)
    {
        return;
    }

    // 情况B：气泡压缩（槽位 0 已消耗，槽位 1 仍待处理）
    if (!pipe_regs.if_id.slots[0].valid && pipe_regs.if_id.slots[1].valid)
    {
        pipe_regs.if_id.slots[0] = pipe_regs.if_id.slots[1];
        pipe_regs.if_id.slots[1].valid = false;

        if (pc < MEMORY_SIZE)
        {
            Word instr = fetch_instr(pc);
            if (!running) return;  // 页错误导致执行中止
            pipe_regs.if_id.slots[1].instr = instr;
            pipe_regs.if_id.slots[1].valid = true;
            pc += 4;
        }
        return;
    }

    // 情况C：两个槽位皆空，取指 8 字节（双指令）
    if (!pipe_regs.if_id.slots[0].valid && !pipe_regs.if_id.slots[1].valid)
    {
        if (pc + 8 <= MEMORY_SIZE)
        {
            // 虚拟 PC 转物理地址，用于双指令取指
            Word satp_val = reg_file.read_csr(CSR_SATP);
            TranslationResult trans = memory.translate(pc, false, true, satp_val);
            if (trans.fault) {
                std::cout << "[Trap] Instruction page fault at PC=0x" << std::hex << pc << std::dec << std::endl;
                handleTrap(pc, trans.cause, pc);
                return;
            }
            Addr phys_pc = trans.paddr;

            // 检查 8 字节双取指不跨越物理页边界
            if ((phys_pc >> 12) != ((phys_pc + 7) >> 12)) {
                // 跨页：退化为单指令取指
                Word instr0 = memory.read(phys_pc, 4);
                pipe_regs.if_id.slots[0].instr = instr0;
                pipe_regs.if_id.slots[0].valid = true;
                pipe_regs.if_id.pc = pc;
                pc += 4;
                return;
            }

            if (phys_pc + 8 <= MEMORY_SIZE) {
                uint64_t dbl_instr = memory.read_double_instr(phys_pc);
                pipe_regs.if_id.slots[0].instr = static_cast<Word>(dbl_instr & 0xFFFFFFFF);
                pipe_regs.if_id.slots[1].instr = static_cast<Word>(dbl_instr >> 32);
                pipe_regs.if_id.slots[0].valid = true;
                pipe_regs.if_id.slots[1].valid = true;
                pipe_regs.if_id.pc = pc;
                pc += 8;
            }
        }
        else if (pc + 4 <= MEMORY_SIZE)
        {
            // 接近内存末端：单指令取指
            Word instr = fetch_instr(pc);
            if (!running) return;
            pipe_regs.if_id.slots[0].instr = instr;
            pipe_regs.if_id.slots[0].valid = true;
            pipe_regs.if_id.pc = pc;
            pc += 4;
        }
    }
}

// =========================================================================
// 中断处理
// =========================================================================
void CPUCore::checkAndHandleInterrupts()
{
    Word mstatus_val = reg_file.read_csr(CSR_MSTATUS);
    Word mie_val     = reg_file.read_csr(CSR_MIE);
    Word mip_val     = reg_file.read_csr(CSR_MIP);

    // 全局中断使能必须已设置
    if (!(mstatus_val & MSTATUS_MIE)) return;

    // 定时器中断：MIE.MTIE && MIP.MTIP
    if ((mie_val & MIE_MTIE) && (mip_val & MIP_MTIP)) {
        stats.interrupt_flushes++;
        std::cout << "[Interrupt] Timer interrupt triggered at PC=0x"
                  << std::hex << pc << std::dec << std::endl;

        // 将 MIE 保存到 MPIE，清除 MIE（防止重入中断）
        Word mstatus_new = mstatus_val & ~MSTATUS_MIE;          // Clear MIE
        mstatus_new = (mstatus_new & ~MSTATUS_MPIE) |
                      ((mstatus_val & MSTATUS_MIE) ? MSTATUS_MPIE : 0);  // MPIE = old MIE
        reg_file.write_csr(CSR_MSTATUS, mstatus_new);

        reg_file.write_csr(CSR_MEPC, pc);
        reg_file.write_csr(CSR_MCAUSE, MCAUSE_TIMER_INT);
        pc = reg_file.read_csr(CSR_MTVEC);
        pipe_regs.flush();
        return;
    }

    // 软件中断：MIE.MSIE && MIP.MSIP
    if ((mie_val & MIE_MSIE) && (mip_val & MIP_MSIP)) {
        stats.interrupt_flushes++;
        std::cout << "[Interrupt] Software interrupt triggered at PC=0x"
                  << std::hex << pc << std::dec << std::endl;

        reg_file.write_csr(CSR_MEPC, pc);
        reg_file.write_csr(CSR_MCAUSE, MCAUSE_MSI_INT);
        pc = reg_file.read_csr(CSR_MTVEC);
        pipe_regs.flush();
        return;
    }

    // 外部中断：MIE.MEIE && MIP.MEIP
    if ((mie_val & MIE_MEIE) && (mip_val & MIP_MEIP)) {
        stats.interrupt_flushes++;
        std::cout << "[Interrupt] External interrupt triggered at PC=0x"
                  << std::hex << pc << std::dec << std::endl;

        reg_file.write_csr(CSR_MEPC, pc);
        reg_file.write_csr(CSR_MCAUSE, MCAUSE_MEI_INT);
        pc = reg_file.read_csr(CSR_MTVEC);
        pipe_regs.flush();
        return;
    }
}

void CPUCore::handleTrap(Addr fault_pc, Word mcause_val, Word mtval_val)
{
    reg_file.write_csr(CSR_MEPC, fault_pc);
    reg_file.write_csr(CSR_MCAUSE, mcause_val);
    reg_file.write_csr(CSR_MTVAL, mtval_val);
    pc = reg_file.read_csr(CSR_MTVEC);

    pipe_regs.flush();
    pipe_regs.ex_mem.slots[0].valid = false;
    pipe_regs.ex_mem.slots[1].valid = false;
}

// =========================================================================
// 调试输出
// =========================================================================
void CPUCore::dumpState() const
{
    std::cout << "\n==========================================" << std::endl;
    std::cout << "         Simulation Performance           " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << " Total Cycles Handled : " << cycle_count << std::endl;
    std::cout << " Total Instructions   : " << instr_count << std::endl;

    if (cycle_count > 0)
    {
        double ipc = static_cast<double>(instr_count) / cycle_count;
        std::cout << " Final IPC            : " << std::fixed << std::setprecision(3) << ipc << std::endl;
    }

    // 流水线详细统计
    if (cycle_count > 0) {
        std::cout << "\n--- Pipeline Statistics ---" << std::endl;
        std::cout << " Dual-Issue Cycles    : " << stats.dual_issues
                  << " (" << std::fixed << std::setprecision(1)
                  << (100.0 * stats.dual_issues / cycle_count) << "%)" << std::endl;
        std::cout << " Single-Issue Cycles  : " << stats.single_issues
                  << " (" << (100.0 * stats.single_issues / cycle_count) << "%)" << std::endl;
        std::cout << " Stall Cycles         : " << stats.stall_cycles
                  << " (" << (100.0 * stats.stall_cycles / cycle_count) << "%)" << std::endl;
        if (stats.branch_flushes > 0)
            std::cout << " Branch Flushes       : " << stats.branch_flushes << std::endl;
        if (stats.load_use_stalls > 0)
            std::cout << " Load-Use Stalls      : " << stats.load_use_stalls << std::endl;
        if (stats.interrupt_flushes > 0)
            std::cout << " Interrupt Flushes    : " << stats.interrupt_flushes << std::endl;
        if (stats.memory_port_conflicts > 0)
            std::cout << " Memory Port Conflicts: " << stats.memory_port_conflicts << std::endl;
    }

    reg_file.dump_registers();
}

void CPUCore::dumpPipeline() const {
    std::cout << "\nCycle: " << cycle_count << " | PC: 0x" << std::hex << pc << std::dec << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    auto printStage = [&](const char* name, const PipelineSlot& s0, const PipelineSlot& s1) {
        std::cout << std::setw(6) << name << " | Slot0: " << std::setw(20) << s0.getDisasm(pc)
            << " | Slot1: " << s1.getDisasm(pc) << std::endl;
        };

    printStage(" [WB] ", pipe_regs.mem_wb.slots[0], pipe_regs.mem_wb.slots[1]);
    printStage(" [MEM]", pipe_regs.ex_mem.slots[0], pipe_regs.ex_mem.slots[1]);
    printStage(" [EX] ", pipe_regs.id_ex.slots[0], pipe_regs.id_ex.slots[1]);
    printStage(" [ID] ", pipe_regs.if_id.slots[0], pipe_regs.if_id.slots[1]);

    std::cout << " [IF]  | Fetching from PC: 0x" << std::hex << pc << std::dec << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;
}
