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
    case CSR_MEDELEG:   return 0;           // 不支持委托
    case CSR_MIDELEG:   return 0;
    case CSR_MIE:       return mie;
    case CSR_MTVEC:     return mtvec;

    // 机器模式陷阱处理
    case CSR_MSCRATCH:  return mscratch;
    case CSR_MEPC:      return mepc;
    case CSR_MCAUSE:    return mcause;
    case CSR_MTVAL:     return mtval;
    case CSR_MIP:       return mip;

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
    case CSR_MEDELEG: break;                             // 不支持
    case CSR_MIDELEG: break;
    case CSR_MIE:     mie = data; break;
    case CSR_MTVEC:   mtvec = data; break;

    // 机器模式陷阱处理
    case CSR_MSCRATCH: mscratch = data; break;
    case CSR_MEPC:     mepc = data; break;
    case CSR_MCAUSE:   mcause = data; break;
    case CSR_MTVAL:    mtval = data; break;

    case CSR_MIP: {
        // MTIP (bit 7) 和 MEIP (bit 11) 只读，由硬件驱动
        // 仅 MSIP (bit 3) 可由软件写入
        Word writable_mask = MIP_MSIP;
        mip = (mip & ~writable_mask) | (data & writable_mask);
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
