#pragma once
#include "common.h"

class RegisterFile
{
private:
    std::array<Word, REG_COUNT> gpr; // x0~x31

    // M-mode CSRs
    Word mstatus = 0;       // M-Mode Status (0x300)
    Word mepc = 0;          // M-Mode Exception PC (0x341)
    Word mtvec = 0;         // Trap Vector Base (0x305)
    Word mcause = 0;        // Trap Cause (0x342)
    Word mie = 0;           // M-Mode Interrupt Enable (0x304)
    Word mip = 0;           // M-Mode Interrupt Pending (0x344)
    Word mtval = 0;         // M-Mode Trap Value (0x343)
    Word mscratch = 0;      // M-Mode Scratch (0x340)

    // 中断/异常委托
    Word medeleg = 0;       // Machine Exception Delegation (0x302)
    Word mideleg = 0;       // Machine Interrupt Delegation (0x303)

    // S-mode CSRs
    Word sstatus = 0;       // S-Mode Status (0x100, 实际是 mstatus 的受限视图)
    Word sepc = 0;          // S-Mode Exception PC (0x141)
    Word stvec = 0;         // S-Mode Trap Vector (0x105)
    Word scause = 0;        // S-Mode Trap Cause (0x142)
    Word stval = 0;         // S-Mode Trap Value (0x143)
    Word sscratch = 0;      // S-Mode Scratch (0x140)
    Word sie = 0;           // S-Mode Interrupt Enable (0x104)
    Word sip = 0;           // S-Mode Interrupt Pending (0x144)

    Word satp = 0;          // Supervisor ATP (0x180)

    Byte privilege = PRV_M; // 当前特权级

public:
    RegisterFile();

    // 4 个读端口（用于双发射操作数获取）
    Word read_rs1(Byte rs1) const { return (rs1 == 0) ? 0 : gpr[rs1]; }
    Word read_rs2(Byte rs2) const { return (rs2 == 0) ? 0 : gpr[rs2]; }
    Word read_rs1_1(Byte rs1) const { return read_rs1(rs1); }
    Word read_rs2_1(Byte rs2) const { return read_rs2(rs2); }

    // 2 个写端口（带写使能）
    void write_rd(Byte rd, Word data, bool regWrite);

    // CSR 读写接口
    Word read_csr(HalfWord csr) const;
    void write_csr(HalfWord csr, Word data);

    // 特权级管理
    Byte get_privilege() const { return privilege; }
    void set_privilege(Byte prv) { privilege = prv; }

    // 硬件置位 MTIP（软件通过 CSR 写入时只读）
    void hw_set_mtip(bool set) {
        if (set) mip |= MIP_MTIP; else mip &= ~MIP_MTIP;
    }

    // 检查中断是否被委托给 S-mode
    bool interrupt_delegated(Word interrupt_cause) const {
        // 中断的 cause 编码：bit31=1，原因码在低 4 位
        // mideleg 的位对应 MIE/MIP 的相同位位置
        // SSIP→bit1, MSIP→bit3, STIP→bit5, MTIP→bit7, SEIP→bit9, MEIP→bit11
        switch (interrupt_cause & 0xF) {
            case 1:  return mideleg & MIP_SSIP;   // Supervisor software
            case 3:  return mideleg & MIP_MSIP;   // Machine software
            case 5:  return mideleg & MIP_STIP;   // Supervisor timer
            case 7:  return mideleg & MIP_MTIP;   // Machine timer
            case 9:  return mideleg & MIP_SEIP;   // Supervisor external
            case 11: return mideleg & MIP_MEIP;   // Machine external
            default: return false;
        }
    }

    // 检查异常是否被委托给 S-mode
    bool exception_delegated(Word exception_cause) const {
        if (exception_cause >= 16) return false;
        return medeleg & (1u << exception_cause);
    }

    // 调试状态输出
    void dump_registers() const;

    const std::array<Word, REG_COUNT>& Getgpr() const { return gpr; }
};
