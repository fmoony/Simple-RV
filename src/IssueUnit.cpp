#include "../include/IssueUnit.h"
#include <iostream>

bool IssueUnit::canIssueTogether(const DecodedData& i0, const DecodedData& i1) const
{
    // 1. Structural hazard: single-port memory, can't dual-issue two mem ops
    if (i0.is_memory && i1.is_memory) {
        return false;
    }

    // 2. Control hazard: branches must be single-issued in slot 0
    if (i0.is_branch || i1.is_branch) {
        return false;
    }

    // 3. Load-Use hazard in same cycle (forwarding can't resolve this)
    if (i0.is_memory && i0.regWrite && i0.rd != 0) {
        if ((i1.uses_rs1 && i1.rs1 == i0.rd) ||
            (i1.uses_rs2 && i1.rs2 == i0.rd)) {
            return false;
        }
    }

    // 4. System instructions must single-issue
    if (i0.is_ecall || i0.is_mret || i0.is_csr ||
        i1.is_ecall || i1.is_mret || i1.is_csr) {
        return false;
    }

    // ALU->ALU RAW is allowed (forwarding resolves it)
    return true;
}

void IssueUnit::decodeAndIssue(IF_ID_Buffer& if_id, ID_EX_Buffer& id_ex,
    const RegisterFile& reg_file, PipelineRegisters& pipe_regs)
{
    if (!if_id.slots[0].valid) return;

    // 1. Decode both slots
    decodeInstruction(if_id.slots[0]);
    decodeInstruction(if_id.slots[1]);

    // 2. Cross-cycle Load-Use stall detection
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
        // Insert bubble: invalidate ID_EX, keep IF_ID unchanged for retry
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

    // 3. Emit to ID_EX
    id_ex.slots[0] = if_id.slots[0];
    id_ex.pc = if_id.pc;
    id_ex.memRead[0] = if_id.slots[0].d.is_memory && if_id.slots[0].d.regWrite;
    id_ex.memWrite[0] = if_id.slots[0].d.is_memory && !if_id.slots[0].d.regWrite;

    if (dualIssue) {
        id_ex.slots[1] = if_id.slots[1];
        id_ex.memRead[1] = if_id.slots[1].d.is_memory && if_id.slots[1].d.regWrite;
        id_ex.memWrite[1] = if_id.slots[1].d.is_memory && !if_id.slots[1].d.regWrite;

        // Clear IF_ID so fetch grabs fresh instructions
        if_id.slots[0].valid = false;
        if_id.slots[1].valid = false;
    }
    else {
        // Single-issue fallback
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

    // Extract fields
    d.op = instr & 0x7F;
    d.rd = (instr >> 7) & 0x1F;
    d.rs1 = (instr >> 15) & 0x1F;
    d.rs2 = (instr >> 20) & 0x1F;
    d.funct3 = (instr >> 12) & 0x7;
    d.funct7 = (instr >> 25) & 0x7F;

    // Reset control signals
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
    case 0x33: // R-type (ADD, SUB, AND, OR, ...)
        d.uses_rs1 = true;
        d.uses_rs2 = true;
        d.regWrite = true;
        break;

    case 0x13: // I-type ALU (ADDI, SLLI, etc.)
        d.uses_rs1 = true;
        d.regWrite = true;
        d.imm = ((int32_t)instr) >> 20; // sign-extend 12-bit immediate
        break;

    case 0x03: // I-type Load (LW, LH, LB, LBU, LHU)
        d.uses_rs1 = true;
        d.regWrite = true;
        d.is_memory = true;
        d.imm = ((int32_t)instr) >> 20;
        break;

    case 0x23: // S-type Store (SW, SH, SB)
        d.uses_rs1 = true;
        d.uses_rs2 = true;
        d.is_memory = true;
        // imm[11:5] = instr[31:25], imm[4:0] = instr[11:7]
        d.imm = (((int32_t)instr) >> 25 << 5) | ((instr >> 7) & 0x1F);
        break;

    case 0x63: // B-type Branch (BEQ, BNE, BLT, BGE, BLTU, BGEU)
        d.uses_rs1 = true;
        d.uses_rs2 = true;
        d.is_branch = true;
        d.imm = (((int32_t)instr >> 31) << 12) |
                (((instr >> 7) & 0x1) << 11) |
                (((instr >> 25) & 0x3F) << 5) |
                (((instr >> 8) & 0xF) << 1);
        break;

    case 0x37: // U-type (LUI)
    case 0x17: // U-type (AUIPC)
        d.regWrite = true;
        d.imm = instr & 0xFFFFF000;
        break;

    case 0x6F: // J-type (JAL)
        d.regWrite = true;
        d.is_branch = true;
        d.imm = (((int32_t)instr >> 31) << 20) |
                (((instr >> 12) & 0xFF) << 12) |
                (((instr >> 20) & 0x1) << 11) |
                (((instr >> 21) & 0x3FF) << 1);
        break;

    case 0x67: // I-type (JALR)
        d.uses_rs1 = true;
        d.regWrite = true;
        d.is_branch = true;
        d.imm = ((int32_t)instr) >> 20;
        break;

    case 0x73: // System instructions
    {
        if (d.funct3 == 0x0) {
            // Privileged: funct12 selects ECALL/EBREAK/MRET
            Word funct12 = (instr >> 20) & 0xFFF;
            if (funct12 == 0x000) {
                d.is_ecall = true;
            } else if (funct12 == 0x302) {
                d.is_mret = true;
            } else if (funct12 == 0x001) {
                // EBREAK: passes through pipeline, handled in WB
            } else if (funct12 == 0x105) {
                // WFI: treated as NOP in this simulator
            } else {
                d.is_illegal = true;
            }
        } else {
            // CSR instructions (funct3 = 1,2,3,5,6,7)
            d.is_csr = true;
            d.csr_addr = (instr >> 20) & 0xFFF;  // 12-bit CSR number
            d.regWrite = (d.rd != 0);             // rd=x0 means no GPR write

            // funct3: 1=CSRRW, 2=CSRRS, 3=CSRRC, 5=CSRRWI, 6=CSRRSI, 7=CSRRCI
            if (d.funct3 <= 3) {
                // Register forms: read rs1 for the write value
                d.uses_rs1 = true;
            }
            // Immediate forms (funct3 5,6,7): zimm in rs1 field (bits 15-19)
            // uses_rs1 stays false; d.rs1 already holds the 5-bit zimm
        }
        break;
    }

    case 0x0F: // FENCE / FENCE.I
        // No-ops in single-hart in-order core
        break;

    default:
        // opcode 0x00 is unprogrammed memory, handled by WB fatal-error check
        if (d.op != 0x00) {
            d.is_illegal = true;
        }
        break;
    }

    slot.rd = d.rd;
    slot.regWrite = d.regWrite;
}
