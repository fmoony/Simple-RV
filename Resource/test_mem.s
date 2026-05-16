.section .text
.globl _start

_start:
    li x1, 100
    li x2, 0x0

    sw x1, 0(x2)
    lw x3, 0(x2)

    li x4, 0x10000000

    li x5, 49      # 默认输出 '1'
    li x6, 100
    bne x3, x6, fail

    li x5, 50      # 成功输出 '2'

fail:
    sb x5, 0(x4)

    j .