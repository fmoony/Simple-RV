.section .text
.globl _start

_start:
    li x1, 0      # i
    li x2, 10     # limit
    li x4, 0x10000000

loop:
    addi x5, x1, 48
    sb x5, 0(x4)

    addi x1, x1, 1
    blt x1, x2, loop

    j .