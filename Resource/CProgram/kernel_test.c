#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20
#define PRINT(s) do { const char* _p = s; for (int _i = 0; _p[_i]; _i++) { \
    while ((UART_LSR & UART_LSR_TX_IDLE) == 0); UART_DAT = _p[_i]; } \
} while(0)

void s_trap_entry(void);

void _start() {
    PRINT("M-mode\n");

    // Delegation: ECALL from S (bit 9) + illegal instr (bit 2) to S-mode
    __asm__ volatile ("csrw 0x302, %0" :: "r"((1<<9)|(1<<2)));

    // stvec
    uint32_t s_handler = (uint32_t)&s_trap_entry;
    __asm__ volatile ("csrw 0x105, %0" :: "r"(s_handler));

    // mstatus: MPP=S, MPIE=1
    uint32_t ms;
    __asm__ volatile ("csrr %0, 0x300" : "=r"(ms));
    ms = (ms & ~0x1800) | (1u << 11); // MPP=S
    ms |= 0x80; // MPIE=1
    __asm__ volatile ("csrw 0x300, %0" :: "r"(ms));

    // mepc = s_entry
    uint32_t s_entry;
    __asm__ volatile ("la %0, s_entry" : "=r"(s_entry));
    __asm__ volatile ("csrw 0x341, %0" :: "r"(s_entry));

    PRINT(" mret\n");
    __asm__ volatile ("mret");

    // S-mode entry
    __asm__ volatile("s_entry:");
    PRINT("S-mode OK\n");

    // Test ECALL from S
    __asm__ volatile ("ecall");
    PRINT("S-ECALL OK\n");

    PRINT("Done\n");
    __asm__ volatile ("ebreak");
}

__attribute__((interrupt("supervisor")))
void s_trap_entry(void) {
    uint32_t scause, sepc;
    __asm__ volatile ("csrr %0, 0x142" : "=r"(scause));
    __asm__ volatile ("csrr %0, 0x141" : "=r"(sepc));
    if ((scause & 0x7FFFFFFF) == 9) {
        sepc += 4;
        __asm__ volatile ("csrw 0x141, %0" :: "r"(sepc));
    }
    __asm__ volatile ("sret");
}
