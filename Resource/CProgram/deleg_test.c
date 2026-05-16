#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20

#define PRINT(s) do { \
    const char* _p = s; \
    for (int _i = 0; _p[_i]; _i++) { \
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0); \
        UART_DAT = _p[_i]; \
    } \
} while(0)

void _start() {
    PRINT("CSR Del Test\n");

    // 测试 MEDELEG 读写
    uint32_t val;
    __asm__ volatile ("csrr %0, 0x302" : "=r"(val));
    __asm__ volatile ("csrw 0x302, %0" :: "r"(0x200));
    __asm__ volatile ("csrr %0, 0x302" : "=r"(val));
    if (val == 0x200) PRINT("MEDELEG OK\n");
    else PRINT("MEDELEG FAIL\n");

    // 测试 MIDELEG 读写
    __asm__ volatile ("csrr %0, 0x303" : "=r"(val));
    __asm__ volatile ("csrw 0x303, %0" :: "r"(0x88));
    __asm__ volatile ("csrr %0, 0x303" : "=r"(val));
    if (val == 0x88) PRINT("MIDELEG OK\n");
    else PRINT("MIDELEG FAIL\n");

    // 测试 S-mode CSR
    __asm__ volatile ("csrw 0x105, %0" :: "r"(0x2000));
    __asm__ volatile ("csrr %0, 0x105" : "=r"(val));
    if (val == 0x2000) PRINT("STVEC OK\n");
    else PRINT("STVEC FAIL\n");

    PRINT("Done\n");
    __asm__ volatile ("ebreak");
}
