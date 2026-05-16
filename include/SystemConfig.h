#pragma once
#include <cstdint>

struct SystemConfig {
    uint32_t ram_size;      // 物理内存总大小 (字节)
    uint32_t pc_init;       // 开机初始 PC (Reset Vector)
    uint32_t sp_init;       // 开机初始栈顶指针 (Stack Pointer)
    uint32_t mtvec_init;    // 默认异常向量基址 (Trap Vector)
    uint32_t uart_base;     // 串口 MMIO 基址

    // 工厂方法：生成默认的 64KB 开发板配置
    static SystemConfig Default64KB() {
        SystemConfig cfg;
        cfg.ram_size = 64 * 1024;        // 64KB
        cfg.pc_init = 0x00000000;       // 入口指令在 0x0000
        cfg.sp_init = 64 * 1024 - 16;   // 栈顶设在物理内存最高处，预留 16 字节安全区
        cfg.mtvec_init = 0x00003000;       // 默认中断入口，可被 CSR 指令覆写
        cfg.uart_base = 0x10000000;       // 串口硬件地址
        return cfg;
    }
};