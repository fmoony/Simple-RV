#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20

static int32_t arr[1024];

void _start() {
    const char* msg = "MemTest Start\n";
    for (int i = 0; msg[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = msg[i];
    }

    // 初始化数组
    for (int32_t i = 0; i < 1024; i++) arr[i] = i;

    // 访存密集：大量数组读写
    int32_t sum = 0;
    for (int32_t k = 0; k < 5000; k++) {
        for (int32_t i = 0; i < 1024; i++) {
            sum += arr[i];
        }
    }

    const char* done = "Done\n";
    for (int i = 0; done[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = done[i];
    }

    // 防止 sum 被优化掉
    UART_DAT = (char)(sum & 0xFF);

    __asm__ volatile ("ebreak");
}
