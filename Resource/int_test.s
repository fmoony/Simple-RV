# RISC-V Timer Interrupt Test
# Configures timer, enables interrupts, handler prints and exits
.attribute arch, "rv32i_zicsr"
.section .text
.global _start
.global int_handler

_start:
    # 1. Set up trap handler address (MTVEC = 0x305)
    la x1, int_handler
    csrw 0x305, x1

    # 2. Set mtimecmp = 500 (trigger after ~500 cycles)
    #    CLINT mtimecmp at MMIO 0x02004000
    lui  x2, 0x02004
    li   x3, 500
    sw   x3, 0(x2)          # mtimecmp low = 500
    sw   x0, 4(x2)          # mtimecmp high = 0

    # 3. Enable timer interrupt (MIE.MTIE = bit 7)
    li x4, 0x80
    csrs 0x304, x4

    # 4. Enable global interrupts (MSTATUS.MIE = bit 3)
    li x5, 0x08
    csrs 0x300, x5

    # 5. Idle loop waiting for interrupt
    li x10, 0               # Counter
idle:
    addi x10, x10, 1
    li x11, 1000000          # Safety limit
    blt x10, x11, idle

    # Timeout: should not reach here
    lui x20, 0x10000
    li  x21, 70             # 'F'
    sb  x21, 0(x20)
    ebreak

.align 4
int_handler:
    # Print 'T' (Timer success)
    lui  x4, 0x10000
    li   x5, 84             # 'T'
    sb   x5, 0(x4)

    # Clear timer interrupt by setting mtimecmp to max
    lui  x6, 0x02004
    li   x7, -1
    sw   x7, 0(x6)

    # Skip past the idle loop to the success path
    # mepc currently points to the interrupted instruction
    # Add enough offset to skip to our exit
    csrr x8, 0x341          # x8 = mepc
    la   x9, test_done      # x9 = test_done address
    csrw 0x341, x9          # mepc = test_done (MRET will jump here)

    mret

test_done:
    # Print 'S' (Success)
    lui  x20, 0x10000
    li   x21, 83            # 'S'
    sb   x21, 0(x20)
    ebreak
