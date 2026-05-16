#pragma once
#include "common.h"

class PipelineRegisters 
{
public:
    IF_ID_Buffer  if_id;
    ID_EX_Buffer  id_ex;
    EX_MEM_Buffer ex_mem;
    MEM_WB_Buffer mem_wb;

    // 冲刷流水线，清空所有缓冲 
    void flush();

    // 重置流水线至初始状态 
    void reset();
};