#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <array>
#include <iostream>
#include <fstream>

// RV32I 核心常量
const uint32_t MEMORY_SIZE = 64 * 1024; // 64KB
const uint32_t REG_COUNT = 32;          // x0-x31

using Word = uint32_t;
using Addr = uint32_t;

using HalfWord = uint16_t;
using Byte = uint8_t;

// --- UART MMIO 地址映射 ---
const Addr UART_TX_ADDR = 0x10000000; // 发送数据寄存器 (Write) / 接收数据寄存器 (Read)
const Addr UART_LSR_ADDR = 0x10000005; // 线路状态寄存器 (Line Status Register)

// --- CLINT MMIO 地址映射 (RISC-V CLINT 标准) ---
const Addr CLINT_BASE      = 0x02000000;
const Addr CLINT_MTIMECMP  = 0x02004000;  // hart 0 mtimecmp (低 32 位)
const Addr CLINT_MTIME     = 0x0200BFF8;  // mtime 低 32 位


// --- 核心数据结构 ---
// 存储指令解码后的静态信息，供流水线各阶段传递
struct DecodedData
{
    Byte op = 0;          // 操作码（低 7 位）
    Byte rs1 = 0;         // 源寄存器 1
    Byte rs2 = 0;         // 源寄存器 2
    Byte rd = 0;          // 目标寄存器
    Byte funct3 = 0;      // funct3 字段 (instr[14:12])
    Byte funct7 = 0;      // funct7 字段 (instr[31:25])
    int32_t imm = 0;      // 符号扩展后的立即数

    // 指令属性标志位
    bool uses_rs1 = false;   // 是否读取 rs1
    bool uses_rs2 = false;   // 是否读取 rs2
    bool regWrite = false;   // 是否需要写回寄存器
    bool is_memory = false;  // 是否为访存指令 (Load/Store)
    bool is_branch = false;  // 是否为跳转指令
    bool is_ecall = false;
    bool is_mret = false;
    bool is_csr = false;        // CSR 指令 (op=0x73, funct3!=0)
    bool is_illegal = false;    // 非法指令
    HalfWord csr_addr = 0;      // 12 位 CSR 地址 (instr[31:20])

    bool predicted_taken = false; // BTFNT 分支预测结果
};

// --- 流水线槽位 ---
// 最小数据单元，支持双发射时两条指令的存储
struct PipelineSlot
{
    Word instr = 0;      // 原始 32 位指令字
    DecodedData d;       // 解码后的详细信息
    Word result = 0;     // 执行阶段计算的结果
    Byte rd = 0;         // 目标寄存器号
    bool valid = false;  // 槽位有效性标志（双发射时可能仅一个有效）
    bool regWrite = false; // 写使能控制信号

    // 专用于控制流跳转的字段
    Addr jump_target = 0;

    // CSR 写入值 (EX 阶段计算，CPUCore::execute 提交)
    Word csr_write_val = 0;

    std::string getDisasm(const Addr pc) const {
        if (!valid) return "--- [Empty] ---";
        char buf[64];
        // 打印十六进制 PC 和机器码
        snprintf(buf, sizeof(buf), "0x%04X: [%08X]", pc, instr);
        return std::string(buf);
    }
};

// --- 流水线阶段间寄存器 (Pipeline Latches) ---

// IF -> ID: 指令取回的双指令及当前 PC
struct IF_ID_Buffer
{
    PipelineSlot slots[2];   // 存储取指阶段的两条指令
    Addr pc = 0;             // 取指时的 PC 地址
    bool flush = false;      // 冲刷标志位
};

// ID -> EX: 传递操作数及访存控制信号
struct ID_EX_Buffer
{
    PipelineSlot slots[2];
    Addr pc = 0;
    bool memRead[2] = { false, false };  // 每条指令对应的读使能
    bool memWrite[2] = { false, false }; // 每条指令对应的写使能
};

// EX -> MEM: 传递计算结果及访存地址
struct EX_MEM_Buffer
{
    PipelineSlot slots[2];
    Addr pc = 0;
    Addr mem_addr[2] = { 0, 0 };     // 访存的目标地址
    Addr mem_data[2] = { 0, 0 };     // 待写入内存的数据
    bool memRead[2] = { false, false };
    bool memWrite[2] = { false, false };
};

// MEM -> WB: 传递结果及写回控制
struct MEM_WB_Buffer
{
    PipelineSlot slots[2];
    bool regWrite[2] = { false, false }; // 写回控制信号
};

// ==========================================
// RISC-V 机器模式 (Machine Mode) CSR 地址映射
// ==========================================
#define CSR_MVENDORID  0xF11  // 供应商 ID
#define CSR_MARCHID    0xF12  // 架构 ID
#define CSR_MIMPID     0xF13  // 实现 ID
#define CSR_MHARTID    0xF14  // 硬件线程 ID

#define CSR_MSTATUS    0x300  // 机器模式状态寄存器
#define CSR_MISA       0x301  // 机器模式指令集扩展
#define CSR_MEDELEG    0x302  // 机器模式异常委托
#define CSR_MIDELEG    0x303  // 机器模式中断委托
#define CSR_MIE        0x304  // 机器模式中断使能
#define CSR_MTVEC      0x305  // 机器模式异常基址寄存器

#define CSR_MSCRATCH   0x340  // 机器模式临时寄存器
#define CSR_MEPC       0x341  // 机器模式异常 PC (保存被中断的 PC)
#define CSR_MCAUSE     0x342  // 机器模式异常原因
#define CSR_SATP       0x180  // 监管者地址转换与保护 (Sv32)

#define CSR_MTVAL      0x343  // 机器模式异常值
#define CSR_MIP        0x344  // 机器模式中断 pending

// --- MSTATUS 位掩码 ---
#define MSTATUS_MIE   0x00000008  // bit 3: 机器模式中断使能
#define MSTATUS_MPIE  0x00000080  // bit 7: 机器模式先前中断使能

// --- MIE / MIP 位掩码 ---
#define MIE_MSIE  0x00000008  // bit 3: 软件中断
#define MIE_MTIE  0x00000080  // bit 7: 定时器中断
#define MIE_MEIE  0x00000800  // bit 11: 外部中断
#define MIP_MSIP  0x00000008
#define MIP_MTIP  0x00000080
#define MIP_MEIP  0x00000800

// --- MCAUSE 异常/中断代码 ---
#define MCAUSE_ILLEGAL          2
#define MCAUSE_BREAKPOINT       3
#define MCAUSE_LOAD_MISALIGNED  4
#define MCAUSE_LOAD_FAULT       5
#define MCAUSE_STORE_MISALIGNED 6
#define MCAUSE_STORE_FAULT      7
#define MCAUSE_ECALL_M         11
#define MCAUSE_INST_PAGE_FAULT 12
#define MCAUSE_LOAD_PAGE_FAULT 13
#define MCAUSE_STORE_PAGE_FAULT 15
#define MCAUSE_TIMER_INT       0x80000007  // bit31=1 + 原因码 7
#define MCAUSE_MSI_INT         0x80000003  // bit31=1 + 原因码 3
#define MCAUSE_MEI_INT         0x8000000B  // bit31=1 + 原因码 11

// --- MISA ---
#define MISA_RV32I  0x40000104  // RV32I 基础 ISA

// --- 流水线性能统计 ---
struct PipelineStats
{
    uint64_t dual_issues = 0;          // 双发射周期数
    uint64_t single_issues = 0;        // 单发射周期数
    uint64_t stall_cycles = 0;         // 停顿周期数（0条指令发射）
    uint64_t branches_predicted = 0;   // 预测的分支总数
    uint64_t branches_mispredicted = 0;// 分支误判次数
    uint64_t branch_flushes = 0;       // 分支导致的流水线冲刷
    uint64_t load_use_stalls = 0;      // Load-Use 冒险停顿
    uint64_t interrupt_flushes = 0;    // 中断导致的流水线冲刷
    uint64_t memory_port_conflicts = 0;// 双访存结构冒险冲突

    void reset() { *this = PipelineStats(); }
};
