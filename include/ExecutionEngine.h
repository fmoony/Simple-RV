#pragma once
#include "common.h"
#include "RegisterFile.h"

class ExecutionEngine
{
public:
    // 执行阶段逻辑：ALU 计算、数据转发、分支执行等
    void execute(
        const ID_EX_Buffer& id_ex,
        EX_MEM_Buffer& ex_mem,
        const MEM_WB_Buffer& mem_wb,
        const RegisterFile& reg_file);

    // 获取操作数，实现四级数据转发 (Forwarding)
    static Word getOperand(
            Byte rs,
            const PipelineSlot& current_slot,
            const ID_EX_Buffer& id_ex,
            const MEM_WB_Buffer& mem_wb,
            const RegisterFile& reg_file,
            const EX_MEM_Buffer& ex_mem,
            int slot_id);

private:
    // 单个槽位的执行逻辑
    void executeSlot(
        const PipelineSlot& in_slot,
        PipelineSlot& out_slot,
        const ID_EX_Buffer& id_ex,
        const MEM_WB_Buffer& mem_wb,
        const RegisterFile& reg_file,
        const EX_MEM_Buffer& ex_mem,
        int alu_id);


    // 各指令类型的具体实现
    void executeRType(const DecodedData& d, Word op1, Word op2, PipelineSlot& out_slot);
    void executeIType(const DecodedData& d, Word op1, PipelineSlot& out_slot, Addr pc);
    void executeJump(const DecodedData& d, Word op1, PipelineSlot& out_slot, Addr pc);
    void executeBranch(const DecodedData& d, Word op1, Word op2, PipelineSlot& out_slot, Addr pc);
    void executeCSR(const DecodedData& d, Word rs1_val, PipelineSlot& out_slot, const RegisterFile& reg_file);
};
