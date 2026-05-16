#include "../include/PipelineRegisters.h"

/**
 * 冲刷流水线 (Flush)
 * 核心逻辑：清空所有流水线阶段的有效位与指令信息
 * 当执行阶段检测到分支跳转成功（Branch Taken）时调用，
 * 用于丢弃已经在流水线中但不再需要执行的后续指令。
 */
void PipelineRegisters::flush()
{
    // 只清空前端的预取和译码缓冲 (杀掉错误预测的年轻指令)
    if_id = IF_ID_Buffer();
    id_ex = ID_EX_Buffer();

    // 绝对不能清空 ex_mem 和 mem_wb！
    // 必须让比分支指令更早进入流水线的老指令安全走完写回阶段
}

/**
 * 重置流水线 (Reset)
 * 核心逻辑：将流水线寄存器组恢复至初始状态
 * 在模拟器启动或硬复位时调用，确保所有控制信号（如写使能、访存使能）均处于安全状态。
 */
void PipelineRegisters::reset()
{
    // 调用全局结构体的默认构造函数，将所有 bool 标志位设为 false，数值设为 0
    if_id = IF_ID_Buffer();
    id_ex = ID_EX_Buffer();
    ex_mem = EX_MEM_Buffer();
    mem_wb = MEM_WB_Buffer();
}