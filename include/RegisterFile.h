#pragma once
#include "common.h"

class RegisterFile
{
private:
    std::array<Word, REG_COUNT> gpr; // x0~x31
    Word mstatus = 0;       // M-Mode Status (0x300)
    Word mepc = 0;          // M-Mode Exception PC (0x341)
    Word mtvec = 0;         // Trap Vector Base (0x305)
    Word mcause = 0;        // Trap Cause (0x342)
    Word mie = 0;           // M-Mode Interrupt Enable (0x304)
    Word mip = 0;           // M-Mode Interrupt Pending (0x344)
    Word mtval = 0;         // M-Mode Trap Value (0x343)
    Word mscratch = 0;      // M-Mode Scratch (0x340)
    Word satp = 0;          // Supervisor ATP (0x180)

public:
    RegisterFile();

    // 4 read ports for dual-issue operand fetch
    Word read_rs1(Byte rs1) const { return (rs1 == 0) ? 0 : gpr[rs1]; }
    Word read_rs2(Byte rs2) const { return (rs2 == 0) ? 0 : gpr[rs2]; }
    Word read_rs1_1(Byte rs1) const { return read_rs1(rs1); }
    Word read_rs2_1(Byte rs2) const { return read_rs2(rs2); }

    // 2 write ports with write-enable
    void write_rd(Byte rd, Word data, bool regWrite);

    // CSR read/write interface
    Word read_csr(HalfWord csr) const;
    void write_csr(HalfWord csr, Word data);

    // Hardware sets MTIP (read-only to software via CSR writes)
    void hw_set_mtip(bool set) {
        if (set) mip |= MIP_MTIP; else mip &= ~MIP_MTIP;
    }

    // State dump for debugging
    void dump_registers() const;

    const std::array<Word, REG_COUNT>& Getgpr() const { return gpr; }
};
