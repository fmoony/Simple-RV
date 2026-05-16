// Minimal timer interrupt test
#include <stdint.h>

#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE 0x20

#define CLINT_MTIMECMP_LO (*((volatile uint32_t*)0x02004000))
#define CLINT_MTIMECMP_HI (*((volatile uint32_t*)0x02004004))
#define CLINT_MTIME_LO    (*((volatile uint32_t*)0x0200BFF8))

volatile uint32_t tick_count = 0;
volatile uint32_t isr_hit = 0;

// Forward declarations
void uart_putc(char c);
void uart_print(const char* s);
void timer_isr(void);

// ==========================================
// _start MUST be the FIRST function defined
// (fno-toplevel-reorder ensures it's at 0x0000)
// ==========================================
void _start() {
    // mtvec = &timer_isr
    uint32_t addr = (uint32_t)&timer_isr;
    __asm__ volatile ("csrw 0x305, %0" :: "r"(addr));

    // MIE.MTIE = bit 7 (0x80 > 5-bit imm, use csrs)
    uint32_t v = 0x80;
    __asm__ volatile ("csrs 0x304, %0" :: "r"(v));

    // MSTATUS.MIE = bit 3 (0x08 fits in 5-bit imm)
    __asm__ volatile ("csrsi 0x300, 0x08");

    uart_print("Waiting for timer...\n");

    // Set timer for ~2000 cycles from now
    uint32_t now = CLINT_MTIME_LO;
    CLINT_MTIMECMP_LO = now + 2000;
    CLINT_MTIMECMP_HI = 0;

    // Wait for ISR to set isr_hit
    while (!isr_hit);

    uart_print("Timer ISR fired!\nDone.\n");

    __asm__ volatile ("ebreak");
}

// ==========================================
// Timer ISR (with interrupt attribute for
// automatic save/restore + mret)
// ==========================================
__attribute__((interrupt("machine")))
void timer_isr(void) {
    isr_hit = 1;
    tick_count++;
    CLINT_MTIMECMP_LO = 0xFFFFFFFF;
    CLINT_MTIMECMP_HI = 0xFFFFFFFF;
}

// ==========================================
// UART helpers (AFTER _start)
// ==========================================
void uart_putc(char c) {
    while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
    UART_DAT = c;
}

void uart_print(const char* s) {
    while (*s) uart_putc(*s++);
}
