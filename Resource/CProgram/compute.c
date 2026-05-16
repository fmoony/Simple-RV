#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20

void _start() {
    const char* msg = "Compute Start\n";
    for (int i = 0; msg[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = msg[i];
    }

    int32_t a = 1, b = 2, result = 0;
    for (int32_t i = 0; i < 200000; i++) {
        a = a + b;
        b = b ^ a;
        result ^= a ^ b;
    }

    // 输出结果校验值，防止编译器优化掉计算循环
    const char* done = "Done\n";
    for (int i = 0; done[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = done[i];
    }
    UART_DAT = (char)((a ^ b ^ result) & 0xFF);

    __asm__ volatile ("ebreak");
}
