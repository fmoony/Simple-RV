#pragma once
#include "common.h"
#include "MemorySystem.h"
#include "RegisterFile.h"
#include "PipelineRegisters.h"
#include "IssueUnit.h"
#include "ExecutionEngine.h"
#include "SystemConfig.h"

class CPUCore
{
private:
    SystemConfig      config;
    MemorySystem      memory;
    RegisterFile      reg_file;
    PipelineRegisters pipe_regs;
    IssueUnit         issue_unit;
    ExecutionEngine   exec_engine;

    Addr     pc = 0x0000;
    uint64_t cycle_count = 0;
    uint64_t instr_count = 0;
    bool     running = true;

    // 中断与异常处理
    void checkAndHandleInterrupts();
    void handleTrap(Addr fault_pc, Word mcause_val, Word mtval_val);

public:
    CPUCore(const SystemConfig& cfg = SystemConfig::Default64KB()) : config(cfg) {}

    void init(const std::string& program_file);

    void tick();
    void run();

    // 5 级流水线
    void writeBack();
    void memoryAccess();
    void execute();
    void decodeAndIssue();
    void fetch();

    void dumpState() const;
    void dumpPipeline() const;
};
