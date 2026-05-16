# RISC-V MMIO UART Test: Hello World!

.section .text
.globl _start

_start:
    # 1. 准备 UART 基地址
    lui  x10, 0x10000       # x10 = 0x10000000 (UART_TX_ADDR)

    # 2. 准备字符串首地址
    # 【修复】：使用 la (Load Address) 伪指令，汇编器会自动算好 lui 和 addi
    la   x11, hello_str     

print_loop:
    lb   x12, 0(x11)        
    beq  x12, x0, end_print 
    sb   x12, 0(x10)        
    addi x11, x11, 1        
    jal  x0, print_loop     

end_print:
    ebreak
    # 【修复】：插入几个 nop 气泡，防止流水线停机前误吃后面的数据
    nop
    nop
    nop
    nop

# -------------------------------------------------------------
# 将字符串放入独立的数据段
.section .rodata
hello_str:
    .asciz "Hello, RISC-V Superscalar World!\n"
