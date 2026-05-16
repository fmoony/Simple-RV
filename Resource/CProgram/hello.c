#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20

void _start() {
    // 直接写 UART，不调函数
    const char* msg = "Hello\n";
    for (int i = 0; msg[i] != '\0'; i++) {
        while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
        UART_DAT = msg[i];
    }
    __asm__ volatile ("ebreak");
}
