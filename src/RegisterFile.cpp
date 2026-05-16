#include "../include/RegisterFile.h"
#include <iomanip>

RegisterFile::RegisterFile()
{
    gpr.fill(0);
}

void RegisterFile::write_rd(Byte rd, Word data, bool regWrite)
{
    if (regWrite && rd != 0 && rd < REG_COUNT)
    {
        gpr[rd] = data;
    }
}

Word RegisterFile::read_csr(HalfWord csr) const
{
    switch (csr)
    {
    // Machine Information Registers (read-only)
    case CSR_MVENDORID: return 0;           // 非商业实现
    case CSR_MARCHID:   return 0;           // 未实现
    case CSR_MIMPID:    return 1;           // 版本 1
    case CSR_MHARTID:   return 0;           // 单核

    // 机器模式陷阱设置
    case CSR_MSTATUS:   return mstatus;
    case CSR_MISA:      return MISA_RV32I;  // RV32I
    case CSR_MEDELEG:   return medeleg;
    case CSR_MIDELEG:   return mideleg;
    case CSR_MIE:       return mie;
    case CSR_MTVEC:     return mtvec;

    // 机器模式陷阱处理
    case CSR_MSCRATCH:  return mscratch;
    case CSR_MEPC:      return mepc;
    case CSR_MCAUSE:    return mcause;
    case CSR_MTVAL:     return mtval;
    case CSR_MIP:       return mip;

    // 监管者模式陷阱设置
    case CSR_SSTATUS:   return sstatus;
    case CSR_SIE:       return sie;
    case CSR_STVEC:     return stvec;

    // 监管者模式陷阱处理
    case CSR_SSCRATCH:  return sscratch;
    case CSR_SEPC:      return sepc;
    case CSR_SCAUSE:    return scause;
    case CSR_STVAL:     return stval;
    case CSR_SIP:       return sip;

    // Supervisor (Sv32 MMU)
    case CSR_SATP:      return satp;

    default:            return 0;
    }
}

void RegisterFile::write_csr(HalfWord csr, Word data)
{
    switch (csr)
    {
    // 只读信息寄存器：静默忽略写入

    // 机器模式陷阱设置
    case CSR_MSTATUS: mstatus = data; break;
    case CSR_MISA:    break;                             // 只读
    case CSR_MEDELEG: {
        // WARL: 仅允许写入实现支持的委托位
        // 支持委托：指令未对齐(0)、指令访问(1)、非法指令(2)、断点(3)、
        //           加载未对齐(4)、加载访问(5)、存储未对齐(6)、存储访问(7)、
        //           ECALL_U(8)、ECALL_S(9)、指令页错误(12)、加载页错误(13)、存储页错误(15)
        Word mask = MEDELEG_INST_MISALIGNED | MEDELEG_INST_ACCESS |
                    MEDELEG_ILLEGAL | MEDELEG_BREAKPOINT |
                    MEDELEG_LOAD_MISALIGNED | MEDELEG_LOAD_ACCESS |
                    MEDELEG_STORE_MISALIGNED | MEDELEG_STORE_ACCESS |
                    MEDELEG_ECALL_U | MEDELEG_ECALL_S |
                    MEDELEG_INST_PAGE_FAULT | MEDELEG_LOAD_PAGE_FAULT |
                    MEDELEG_STORE_PAGE_FAULT;
        medeleg = data & mask;
        break;
    }
    case CSR_MIDELEG: {
        // WARL: 仅允许写入实现支持的委托位
        // 支持委托：SSIP(1), MSIP(3), STIP(5), MTIP(7), SEIP(9), MEIP(11)
        Word mask = MIP_SSIP | MIP_MSIP | MIP_STIP | MIP_MTIP | MIP_SEIP | MIP_MEIP;
        mideleg = data & mask;
        break;
    }
    case CSR_MIE:     mie = data; break;
    case CSR_MTVEC:   mtvec = data; break;

    // 机器模式陷阱处理
    case CSR_MSCRATCH: mscratch = data; break;
    case CSR_MEPC:     mepc = data; break;
    case CSR_MCAUSE:   mcause = data; break;
    case CSR_MTVAL:    mtval = data; break;

    case CSR_MIP: {
        // MTIP (bit 7) 和 MEIP (bit 11) 只读，由硬件驱动
        // MSIP (bit 3) 由软件写入
        Word writable_mask = MIP_MSIP;
        mip = (mip & ~writable_mask) | (data & writable_mask);
        break;
    }

    // 监管者模式陷阱设置
    case CSR_SSTATUS: {
        // sstatus 是 mstatus 的受限视图
        // 可写位：SIE, SPIE, SPP（bits 1,5,8）
        Word mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP;
        sstatus = (sstatus & ~mask) | (data & mask);
        // 同步到 mstatus 的对应位
        mstatus = (mstatus & ~mask) | (data & mask);
        break;
    }
    case CSR_SIE: {
        // sie 是 mie 的受限视图（仅 S-level 中断位可写）
        Word mask = MIE_SSIE | MIE_STIE | MIE_SEIE;
        sie = (sie & ~mask) | (data & mask);
        break;
    }
    case CSR_STVEC:   stvec = data; break;

    // 监管者模式陷阱处理
    case CSR_SSCRATCH: sscratch = data; break;
    case CSR_SEPC:     sepc = data; break;
    case CSR_SCAUSE:   scause = data; break;
    case CSR_STVAL:    stval = data; break;

    case CSR_SIP: {
        // SSIP (bit 1) 可由软件写入
        Word writable_mask = MIP_SSIP;
        sip = (sip & ~writable_mask) | (data & writable_mask);
        break;
    }

    // Supervisor (Sv32 MMU)
    case CSR_SATP: satp = data; break;

    default: break;
    }
}

void RegisterFile::dump_registers() const
{
    std::cout << "\n--- Register Dump (Hex) ---" << std::endl;
    for (int i = 0; i < (int)REG_COUNT; ++i)
    {
        std::cout << "x" << std::setfill(' ') << std::setw(2) << std::dec << i << ": 0x"
            << std::setfill('0') << std::setw(8) << std::hex << gpr[i] << "  ";

        if ((i + 1) % 4 == 0)
        {
            std::cout << std::endl;
        }
    }
    std::cout << "---------------------------" << std::dec << std::endl;
}
