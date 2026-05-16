#pragma once
#include "common.h"
#include "PipelineRegisters.h"
#include "RegisterFile.h"

class IssueUnit 
{
public:
    // 检查两条指令是否可以同时发射 (数据/结构/跳转冲突检查)
    bool canIssueTogether(const DecodedData& i0, const DecodedData& i1,
                          PipelineStats& stats) const;

    // 译码与发射核心逻辑，衔接 IF 和 EX 阶段
    void decodeAndIssue(IF_ID_Buffer& if_id, ID_EX_Buffer& id_ex,
        const RegisterFile& reg_file, PipelineRegisters& pipe_regs,
        PipelineStats& stats);

private:
    // 解析 RISC-V 32I 指令信息，slot_pc 用于分支预测
    void decodeInstruction(PipelineSlot& slot, Addr slot_pc);
};