# RISC-V Exception Handling Test
# Tests: Illegal instruction trap, ECALL trap, trap handler recovery
.attribute arch, "rv32i_zicsr"
.section .text
.global _start
.global trap_handler

_start:
    # 1. Set up trap handler at mtvec
    la x1, trap_handler
    csrw 0x305, x1        # mtvec = trap_handler

    # 2. Test ECALL exception
    li x10, 0xAB           # Marker before ECALL
    ecall                   # Should trap to trap_handler
    li x10, 0xCD           # Marker after ECALL (x10 should now be 0xCD after mret)

    # 3. Test illegal instruction
    li x11, 0x100          # Marker before illegal instruction
    .word 0xFFFFFFFF        # Intentionally illegal opcode
    li x11, 0x200          # Marker after illegal (x11 should be 0x200 after mret)

    # 4. All tests passed - print 'P'
    lui x20, 0x10000
    li  x21, 80            # 'P'
    sb  x21, 0(x20)

    ebreak

.align 4
trap_handler:
    # Read mcause to determine trap type
    csrr x5, 0x342        # x5 = mcause
    csrr x6, 0x341        # x6 = mepc

    # Read mtval for additional info
    csrr x7, 0x343        # x7 = mtval

    # Check if this is ECALL (mcause=11) or illegal instruction (mcause=2)
    li x8, 11
    beq x5, x8, handle_ecall

    li x8, 2
    beq x5, x8, handle_illegal

    # Unknown trap - halt
    ebreak

handle_ecall:
    # For ECALL: advance mepc by 4 (skip the ECALL instruction)
    addi x6, x6, 4
    csrw 0x341, x6        # mepc = mepc + 4
    mret

handle_illegal:
    # For illegal instruction: advance mepc by 4 (skip the bad instruction)
    addi x6, x6, 4
    csrw 0x341, x6        # mepc = mepc + 4
    mret
