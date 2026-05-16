.section .text
.globl _start

_start:
    li x1, 5
    li x2, 5

    beq x1, x2, equal

    li x3, 48      # '0'
    j print

equal:
    li x3, 49      # '1'

print:
    li x4, 0x10000000
    sb x3, 0(x4)

    j .