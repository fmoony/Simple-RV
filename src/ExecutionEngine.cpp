#include "../include/ExecutionEngine.h"

void ExecutionEngine::execute(
    const ID_EX_Buffer& id_ex,
    EX_MEM_Buffer& ex_mem,
    const MEM_WB_Buffer& mem_wb,
    const RegisterFile& reg_file)
{
    EX_MEM_Buffer next_ex_mem;
    next_ex_mem.pc = id_ex.pc;
    next_ex_mem.slots[0].valid = false;
    next_ex_mem.slots[1].valid = false;

    for (int i = 0; i < 2; ++i) {
        if (!id_ex.slots[i].valid) continue;

        executeSlot(id_ex.slots[i], next_ex_mem.slots[i], id_ex, mem_wb, reg_file, ex_mem, i);

        next_ex_mem.slots[i].valid = true;
        next_ex_mem.memRead[i] = id_ex.memRead[i];
        next_ex_mem.memWrite[i] = id_ex.memWrite[i];

        if (id_ex.slots[i].d.is_memory) {
            next_ex_mem.mem_addr[i] = next_ex_mem.slots[i].result;
            next_ex_mem.mem_data[i] = getOperand(id_ex.slots[i].d.rs2, id_ex.slots[i], id_ex, mem_wb, reg_file, ex_mem, i);
        }

        // Intra-cycle forwarding: slot 0 result visible to slot 1 immediately
        if (i == 0) {
            ex_mem.slots[0] = next_ex_mem.slots[0];
        }
    }

    ex_mem = next_ex_mem;
}

void ExecutionEngine::executeSlot(
    const PipelineSlot& in_slot,
    PipelineSlot& out_slot,
    const ID_EX_Buffer& id_ex,
    const MEM_WB_Buffer& mem_wb,
    const RegisterFile& reg_file,
    const EX_MEM_Buffer& ex_mem,
    int slot_id)
{
    out_slot = in_slot;
    const DecodedData& d = in_slot.d;

    Addr current_pc = id_ex.pc + (slot_id * 4);

    // CSR instructions: read old CSR value, compute new write value
    if (d.is_csr) {
        Word rs1_val = d.uses_rs1 ?
            getOperand(d.rs1, in_slot, id_ex, mem_wb, reg_file, ex_mem, slot_id) : 0;
        executeCSR(d, rs1_val, out_slot, reg_file);
        return;
    }

    Word op1 = getOperand(d.rs1, in_slot, id_ex, mem_wb, reg_file, ex_mem, slot_id);
    Word op2 = (d.op == 0x33 || d.is_branch) ?
        getOperand(d.rs2, in_slot, id_ex, mem_wb, reg_file, ex_mem, slot_id) : (Word)d.imm;

    if (d.op == 0x6F || d.op == 0x67) {
        if (slot_id == 0) {
            executeJump(d, op1, out_slot, current_pc);
        }
    }
    else if (d.is_branch) {
        if (slot_id == 0) executeBranch(d, op1, op2, out_slot, current_pc);
    }
    else if (d.op == 0x33) {
        executeRType(d, op1, op2, out_slot);
    }
    else {
        executeIType(d, op1, out_slot, current_pc);
    }
}

Word ExecutionEngine::getOperand(
    Byte rs,
    const PipelineSlot& current_slot,
    const ID_EX_Buffer& id_ex,
    const MEM_WB_Buffer& mem_wb,
    const RegisterFile& reg_file,
    const EX_MEM_Buffer& ex_mem,
    int slot_id)
{
    if (rs == 0) return 0;

    // Priority 0: Intra-cycle forwarding (slot 0 -> slot 1 in same cycle)
    if (slot_id == 1) {
        if (ex_mem.slots[0].valid &&
            ex_mem.slots[0].d.regWrite &&
            ex_mem.slots[0].d.rd == rs)
        {
            return ex_mem.slots[0].result;
        }
    }

    // Priority 1: EX_MEM stage (previous cycle results, excluding loads)
    for (int i = 1; i >= 0; --i) {
        if (ex_mem.slots[i].valid && ex_mem.slots[i].d.regWrite && ex_mem.slots[i].d.rd == rs && i != slot_id) {
            if (ex_mem.slots[i].d.is_memory) {
                continue;  // Load data not ready yet; fall through to MEM_WB
            }
            return ex_mem.slots[i].result;
        }
    }

    // Priority 2: MEM_WB stage (about to write back)
    for (int i = 1; i >= 0; --i) {
        if (mem_wb.slots[i].valid &&
            mem_wb.slots[i].regWrite &&
            mem_wb.slots[i].rd == rs)
        {
            return mem_wb.slots[i].result;
        }
    }

    // Priority 3: Register file
    return reg_file.read_rs1(rs);
}

void ExecutionEngine::executeRType(const DecodedData& d, Word op1, Word op2, PipelineSlot& out_slot)
{
    Word funct3 = d.funct3;
    Word funct7 = d.funct7;

    switch (funct3)
    {
        case 0x0: // SUB / ADD
            out_slot.result = (funct7 == 0x20) ? (op1 - op2) : (op1 + op2);
            break;
        case 0x1: // SLL
            out_slot.result = op1 << (op2 & 0x1F);
            break;
        case 0x2: // SLT
            out_slot.result = ((int32_t)op1 < (int32_t)op2) ? 1 : 0;
            break;
        case 0x3: // SLTU
            out_slot.result = (op1 < op2) ? 1 : 0;
            break;
        case 0x4: // XOR
            out_slot.result = op1 ^ op2;
            break;
        case 0x5: // SRA / SRL
            out_slot.result = (funct7 == 0x20) ? ((int32_t)op1 >> (op2 & 0x1F)) : (op1 >> (op2 & 0x1F));
            break;
        case 0x6: // OR
            out_slot.result = op1 | op2;
            break;
        case 0x7: // AND
            out_slot.result = op1 & op2;
            break;
        default:
            out_slot.result = 0;
            break;
    }
}

void ExecutionEngine::executeIType(const DecodedData& d, Word op1, PipelineSlot& out_slot, Addr pc)
{
    Word funct3 = d.funct3;
    Word funct7 = d.funct7;
    Word imm = (Word)d.imm;
    Word shamt = imm & 0x1F;

    switch (d.op) {
    case 0x13: // ALU immediate
        switch (funct3) {
        case 0x0: out_slot.result = op1 + imm; break;                                // ADDI
        case 0x1: out_slot.result = op1 << shamt; break;                             // SLLI
        case 0x2: out_slot.result = ((int32_t)op1 < (int32_t)imm) ? 1 : 0; break;    // SLTI
        case 0x3: out_slot.result = (op1 < imm) ? 1 : 0; break;                      // SLTIU
        case 0x4: out_slot.result = op1 ^ imm; break;                                // XORI
        case 0x5: out_slot.result = (funct7 == 0x20) ? ((int32_t)op1 >> shamt) : (op1 >> shamt); break; // SRAI / SRLI
        case 0x6: out_slot.result = op1 | imm; break;                                // ORI
        case 0x7: out_slot.result = op1 & imm; break;                                // ANDI
        }
        break;
    case 0x03: // Load
    case 0x23: // Store
        out_slot.result = op1 + imm; // effective address
        break;
    case 0x37: // LUI
        out_slot.result = imm;
        break;
    case 0x17: // AUIPC
        out_slot.result = pc + imm;
        break;
    default:
        break;
    }
}

void ExecutionEngine::executeJump(const DecodedData& d, Word op1, PipelineSlot& out_slot, Addr pc)
{
    out_slot.d.is_branch = true;
    out_slot.result = pc + 4;  // return address to rd

    if (d.op == 0x6F) {
        // JAL: target = pc + imm
        out_slot.jump_target = pc + (Word)d.imm;
    }
    else if (d.op == 0x67) {
        // JALR: target = (rs1 + imm) & ~1
        out_slot.jump_target = (op1 + (Word)d.imm) & ~1;
    }
}

void ExecutionEngine::executeBranch(const DecodedData& d, Word op1, Word op2, PipelineSlot& out_slot, Addr pc)
{
    Word funct3 = d.funct3;
    bool taken = false;

    int32_t s_op1 = static_cast<int32_t>(op1);
    int32_t s_op2 = static_cast<int32_t>(op2);

    switch (funct3)
    {
    case 0x0: taken = (op1 == op2); break; // BEQ
    case 0x1: taken = (op1 != op2); break; // BNE
    case 0x4: taken = (s_op1 < s_op2); break;   // BLT
    case 0x5: taken = (s_op1 >= s_op2); break;  // BGE
    case 0x6: taken = (op1 < op2); break;       // BLTU
    case 0x7: taken = (op1 >= op2); break;      // BGEU
    default:  taken = false; break;
    }

    out_slot.d.is_branch = true;

    if (taken) {
        out_slot.jump_target = pc + (Word)d.imm;
    }
    else {
        out_slot.jump_target = pc + 4;
        out_slot.d.is_branch = false;  // not taken, no pipeline flush
    }
}

void ExecutionEngine::executeCSR(const DecodedData& d, Word rs1_val, PipelineSlot& out_slot, const RegisterFile& reg_file)
{
    // Read old CSR value (returned as GPR result)
    Word old_csr = reg_file.read_csr(d.csr_addr);

    // 5-bit zero-extended immediate for CSRRWI/CSRRSI/CSRRCI
    Word zimm = d.rs1 & 0x1F;

    switch (d.funct3) {
    case 1: // CSRRW: atomic read/write
        out_slot.csr_write_val = rs1_val;
        break;
    case 2: // CSRRS: atomic read and set bits
        out_slot.csr_write_val = (d.rs1 != 0) ? (old_csr | rs1_val) : old_csr;
        break;
    case 3: // CSRRC: atomic read and clear bits
        out_slot.csr_write_val = (d.rs1 != 0) ? (old_csr & ~rs1_val) : old_csr;
        break;
    case 5: // CSRRWI: atomic read/write immediate
        out_slot.csr_write_val = zimm;
        break;
    case 6: // CSRRSI: atomic read and set bits immediate
        out_slot.csr_write_val = (zimm != 0) ? (old_csr | zimm) : old_csr;
        break;
    case 7: // CSRRCI: atomic read and clear bits immediate
        out_slot.csr_write_val = (zimm != 0) ? (old_csr & ~zimm) : old_csr;
        break;
    default:
        out_slot.csr_write_val = old_csr;
        break;
    }

    // Result written to rd is the old CSR value
    out_slot.result = old_csr;
}
