#include "../include/IssueUnit.h"
#include <iostream>

bool IssueUnit::canIssueTogether(const DecodedData& i0, const DecodedData& i1) const
{
    // 1. 结构冒险：单端口内存，不能同时发射两条访存指令
    if (i0.is_memory && i1.is_memory) {
        return false;
    }

    // 2. 控制冒险：分支/跳转必须单独在槽位 0 发射
    if (i0.is_branch || i1.is_branch) {
        return false;
    }

    // 3. 同周期 Load-Use 冒险（转发无法解决）
    if (i0.is_memory && i0.regWrite && i0.rd != 0) {
        if ((i1.uses_rs1 && i1.rs1 == i0.rd) ||
            (i1.uses_rs2 && i1.rs2 == i0.rd)) {
            return false;
        }
    }

    // 4. 系统指令必须单独发射
    if (i0.is_ecall || i0.is_mret || i0.is_csr ||
        i1.is_ecall || i1.is_mret || i1.is_csr) {
        return false;
    }

    // ALU→ALU RAW 允许（转发可解决）
    return true;
}

void IssueUnit::decodeAndIssue(IF_ID_Buffer& if_id, ID_EX_Buffer& id_ex,
    const RegisterFile& reg_file, PipelineRegisters& pipe_regs)
{
    if (!if_id.slots[0].valid) return;

    // 1. 解码两个槽位
    decodeInstruction(if_id.slots[0]);
    decodeInstruction(if_id.slots[1]);

    // 2. 跨周期 Load-Use 停顿检测
    bool cross_cycle_stall = false;
    for (int i = 0; i < 2; ++i) {
        if (id_ex.slots[i].valid && id_ex.slots[i].d.is_memory && id_ex.slots[i].d.regWrite) {
            Byte load_rd = id_ex.slots[i].d.rd;
            if (load_rd != 0) {
                if ((if_id.slots[0].d.uses_rs1 && if_id.slots[0].d.rs1 == load_rd) ||
                    (if_id.slots[0].d.uses_rs2 && if_id.slots[0].d.rs2 == load_rd)) {
                    cross_cycle_stall = true;
                }
                if (if_id.slots[1].valid) {
                    if ((if_id.slots[1].d.uses_rs1 && if_id.slots[1].d.rs1 == load_rd) ||
                        (if_id.slots[1].d.uses_rs2 && if_id.slots[1].d.rs2 == load_rd)) {
                        cross_cycle_stall = true;
                    }
                }
            }
        }
    }

    if (cross_cycle_stall) {
        // 插入气泡：无效化 ID_EX，保持 IF_ID 不变以重试
        id_ex.slots[0].valid = false;
        id_ex.slots[1].valid = false;
        id_ex.memRead[0] = false; id_ex.memWrite[0] = false;
        id_ex.memRead[1] = false; id_ex.memWrite[1] = false;
        return;
    }

    bool dualIssue = false;
    if (if_id.slots[1].valid) {
        dualIssue = canIssueTogether(if_id.slots[0].d, if_id.slots[1].d);
    }

    // 3. 送入 ID_EX
    id_ex.slots[0] = if_id.slots[0];
    id_ex.pc = if_id.pc;
    id_ex.memRead[0] = if_id.slots[0].d.is_memory && if_id.slots[0].d.regWrite;
    id_ex.memWrite[0] = if_id.slots[0].d.is_memory && !if_id.slots[0].d.regWrite;

    if (dualIssue) {
        id_ex.slots[1] = if_id.slots[1];
        id_ex.memRead[1] = if_id.slots[1].d.is_memory && if_id.slots[1].d.regWrite;
        id_ex.memWrite[1] = if_id.slots[1].d.is_memory && !if_id.slots[1].d.regWrite;

        // 清空 IF_ID，使取指阶段获取新指令
        if_id.slots[0].valid = false;
        if_id.slots[1].valid = false;
    }
    else {
        // 单发射回退
        id_ex.slots[1].valid = false;
        id_ex.memRead[1] = false;
        id_ex.memWrite[1] = false;

        // Bubble compress: shift slot 1 -> slot 0
        if_id.slots[0] = if_id.slots[1];
        if_id.pc = if_id.pc + 4;
        if_id.slots[1].valid = false;
    }
}

void IssueUnit::decodeInstruction(PipelineSlot& slot)
{
    Word instr = slot.instr;
    DecodedData& d = slot.d;

    // 提取指令字段
    d.op = instr & 0x7F;
    d.rd = (instr >> 7) & 0x1F;
    d.rs1 = (instr >> 15) & 0x1F;
    d.rs2 = (instr >> 20) & 0x1F;
    d.funct3 = (instr >> 12) & 0x7;
    d.funct7 = (instr >> 25) & 0x7F;

    // 复位控制信号
    d.uses_rs1 = false;
    d.uses_rs2 = false;
    d.regWrite = false;
    d.is_memory = false;
    d.is_branch = false;
    d.is_ecall = false;
    d.is_mret = false;
    d.is_csr = false;
    d.is_illegal = false;
    d.csr_addr = 0;
    d.imm = 0;

    switch (d.op) {
    case 0x33: // R 型 (ADD, SUB, AND, OR, ...)
        d.uses_rs1 = true;
        d.uses_rs2 = true;
        d.regWrite = true;
        break;

    case 0x13: // I 型 ALU (ADDI, SLLI, etc.)
        d.uses_rs1 = true;
        d.regWrite = true;
        d.imm = ((int32_t)instr) >> 20; // 12 位立即数符号扩展
        break;

    case 0x03: // I 型 Load (LW, LH, LB, LBU, LHU)
        d.uses_rs1 = true;
        d.regWrite = true;
        d.is_memory = true;
        d.imm = ((int32_t)instr) >> 20;
        break;

    case 0x23: // S 型 Store (SW, SH, SB)
        d.uses_rs1 = true;
        d.uses_rs2 = true;
        d.is_memory = true;
        // imm[11:5] = instr[31:25], imm[4:0] = instr[11:7]
        d.imm = (((int32_t)instr) >> 25 << 5) | ((instr >> 7) & 0x1F);
        break;

    case 0x63: // B 型 Branch (BEQ, BNE, BLT, BGE, BLTU, BGEU)
        d.uses_rs1 = true;
        d.uses_rs2 = true;
        d.is_branch = true;
        d.imm = (((int32_t)instr >> 31) << 12) |
                (((instr >> 7) & 0x1) << 11) |
                (((instr >> 25) & 0x3F) << 5) |
                (((instr >> 8) & 0xF) << 1);
        break;

    case 0x37: // U 型 (LUI)
    case 0x17: // U 型 (AUIPC)
        d.regWrite = true;
        d.imm = instr & 0xFFFFF000;
        break;

    case 0x6F: // J 型 (JAL)
        d.regWrite = true;
        d.is_branch = true;
        d.imm = (((int32_t)instr >> 31) << 20) |
                (((instr >> 12) & 0xFF) << 12) |
                (((instr >> 20) & 0x1) << 11) |
                (((instr >> 21) & 0x3FF) << 1);
        break;

    case 0x67: // I 型 (JALR)
        d.uses_rs1 = true;
        d.regWrite = true;
        d.is_branch = true;
        d.imm = ((int32_t)instr) >> 20;
        break;

    case 0x73: // 系统指令
    {
        if (d.funct3 == 0x0) {
            // 特权指令：funct12 选择 ECALL/EBREAK/MRET
            Word funct12 = (instr >> 20) & 0xFFF;
            if (funct12 == 0x000) {
                d.is_ecall = true;
            } else if (funct12 == 0x302) {
                d.is_mret = true;
            } else if (funct12 == 0x001) {
                // EBREAK：穿过流水线，在 WB 阶段处理
            } else if (funct12 == 0x105) {
                // WFI：在此模拟器中视为 NOP
            } else {
                d.is_illegal = true;
            }
        } else {
            // CSR 指令 (funct3 = 1,2,3,5,6,7)
            d.is_csr = true;
            d.csr_addr = (instr >> 20) & 0xFFF;  // 12 位 CSR 编号
            d.regWrite = (d.rd != 0);             // rd=x0 表示不写 GPR

            // funct3: 1=CSRRW, 2=CSRRS, 3=CSRRC, 5=CSRRWI, 6=CSRRSI, 7=CSRRCI
            if (d.funct3 <= 3) {
                // 寄存器形式：读取 rs1 作为写入值
                d.uses_rs1 = true;
            }
            // 立即数形式 (funct3 5,6,7)：zimm 位于 rs1 字段 (bits 15-19)
            // uses_rs1 保持 false；d.rs1 已经持有 5 位 zimm
        }
        break;
    }

    case 0x0F: // FENCE / FENCE.I（视为 NOP）
        // 在单核顺序核心中视为 NOP
        break;

    default:
        // opcode 0x00 是未编程内存，由 WB 阶段的致命错误检查处理
        if (d.op != 0x00) {
            d.is_illegal = true;
        }
        break;
    }

    slot.rd = d.rd;
    slot.regWrite = d.regWrite;
}
