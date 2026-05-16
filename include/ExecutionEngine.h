#pragma once
#include "common.h"
#include "RegisterFile.h"

class ExecutionEngine 
{
public:
    // ִ�н׶��߼�������������·������ִ������ָ�� [cite: 42, 48]
    void execute(
        const ID_EX_Buffer& id_ex, 
        EX_MEM_Buffer& ex_mem, 
        const MEM_WB_Buffer& mem_wb,
        const RegisterFile& reg_file);

    // ��ȡ������������������· (Forwarding) [cite: 45, 48]
    static Word getOperand(        
            Byte rs,
            const PipelineSlot& current_slot,
            const ID_EX_Buffer& id_ex,
            const MEM_WB_Buffer& mem_wb,
            const RegisterFile& reg_file,
            const EX_MEM_Buffer& ex_mem,
            int slot_id);

private:
    // ������λ��ִ���߼� [cite: 46, 48]
    void executeSlot(
        const PipelineSlot& in_slot, 
        PipelineSlot& out_slot,
        const ID_EX_Buffer& id_ex, 
        const MEM_WB_Buffer& mem_wb, 
        const RegisterFile& reg_file,
        const EX_MEM_Buffer& ex_mem,
        int alu_id);


    // ����ָ�����͵�����ʵ�� [cite: 46, 48]
    void executeRType(const DecodedData& d, Word op1, Word op2, PipelineSlot& out_slot);
    void executeIType(const DecodedData& d, Word op1, PipelineSlot& out_slot, Addr pc);
    void executeJump(const DecodedData& d, Word op1, PipelineSlot& out_slot, Addr pc);
    void executeBranch(const DecodedData& d, Word op1, Word op2, PipelineSlot& out_slot, Addr pc);
    void executeCSR(const DecodedData& d, Word rs1_val, PipelineSlot& out_slot, const RegisterFile& reg_file);
};