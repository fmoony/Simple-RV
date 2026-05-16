#include <stdint.h>
#include <stdbool.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20

void _start() {
    const char* msg = "BranchTest Start\n";
    for (int i = 0; msg[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = msg[i];
    }

    int32_t a = 0, b = 1, c = 2, d = 3;
    int32_t cnt = 0;

    // 分支密集：随机决策树
    for (int32_t i = 0; i < 100000; i++) {
        int32_t x = a ^ b ^ c ^ d;
        if (x & 1) {
            a = a + 1;
            if (x & 2) b = b ^ c;
            else b = b + d;
        } else {
            c = c - 1;
            if (x & 4) d = d ^ a;
            else d = d + b;
        }
        if (x & 8) {
            if (x & 16) a = a ^ d;
            else c = c ^ b;
        } else {
            if (x & 32) b = b ^ a;
            else d = d ^ c;
        }
        cnt++;
    }

    const char* done = "Done\n";
    for (int i = 0; done[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = done[i];
    }

    UART_DAT = (char)((a ^ b ^ c ^ d ^ cnt) & 0xFF);
    __asm__ volatile ("ebreak");
}
