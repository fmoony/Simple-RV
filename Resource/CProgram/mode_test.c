#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20
#define PRINT(s) do { const char* _p = s; for (int _i = 0; _p[_i]; _i++) { \
    while ((UART_LSR & UART_LSR_TX_IDLE) == 0); UART_DAT = _p[_i]; } \
} while(0)

void s_trap_entry(void);

void _start() {
    PRINT("M-mode Boot\n");

    // medeleg: ECALL from S-mode (bit 9) to S-mode
    __asm__ volatile ("csrw 0x302, %0" :: "r"(1 << 9));

    // stvec = &s_trap_entry
    uint32_t s_handler = (uint32_t)&s_trap_entry;
    __asm__ volatile ("csrw 0x105, %0" :: "r"(s_handler));
    PRINT("stvec set\n");

    // mstatus: MPP=S (bits 12:11 = 01), MPIE=1 (bit 7)
    uint32_t mstatus;
    __asm__ volatile ("csrr %0, 0x300" : "=r"(mstatus));
    mstatus &= ~0x1800;           // Clear MPP
    mstatus |= (1u << 11);     // MPP = S (01)
    mstatus |= 0x80;              // MPIE = 1
    __asm__ volatile ("csrw 0x300, %0" :: "r"(mstatus));

    // mepc = s_mode_entry
    uint32_t s_entry;
    __asm__ volatile ("la %0, s_mode_entry" : "=r"(s_entry));
    __asm__ volatile ("csrw 0x341, %0" :: "r"(s_entry));

    PRINT("mret...\n");
    __asm__ volatile ("mret");

    // Should not reach here
    PRINT("FAIL: back in M!\n");
    __asm__ volatile ("ebreak");

    __asm__ volatile("s_mode_entry:");
    PRINT("S-mode OK\n");

    // ECALL from S-mode (should trap to S-mode handler)
    __asm__ volatile ("ecall");

    PRINT("S-Trap returned OK\n");
    PRINT("Done\n");
    __asm__ volatile ("ebreak");
}

__attribute__((interrupt("supervisor")))
void s_trap_entry(void) {
    uint32_t scause, sepc;
    __asm__ volatile ("csrr %0, 0x142" : "=r"(scause));
    __asm__ volatile ("csrr %0, 0x141" : "=r"(sepc));
    if ((scause & 0x7FFFFFFF) == 9) {
        sepc += 4; // skip ECALL
        __asm__ volatile ("csrw 0x141, %0" :: "r"(sepc));
    }
    __asm__ volatile ("sret");
}
