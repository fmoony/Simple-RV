# RISC-V 32I Fibonacci Test
# 计算斐波那契数列前 20 项并存入内存 0x2000 起始位置
# 目标：测试双发射、数据旁路、访存及循环控制

.section .text
.globl _start

_start:
    # --- 初始化寄存器 ---
    li x1, 20          # n = 20 (计算项数)
    li x2, 0x2000      # 内存起始地址 (sp/x2 通常作为栈，这里暂作数据指针)
    li x3, 0           # F(0) = 0
    li x4, 1           # F(1) = 1
    
    # 将前两项存入内存
    sw x3, 0(x2)       # Store F(0)
    sw x4, 4(x2)       # Store F(1)
    
    li x5, 2           # loop counter i = 2
    addi x2, x2, 8     # 移动指针到 F(2) 的位置

loop:
    # --- 核心计算逻辑 ---
    # 这里的指令流设计可以触发双发射旁路
    add x6, x3, x4     # x6 = F(i-1) + F(i-2)
    sw  x6, 0(x2)      # 将结果存入当前内存地址
    
    # 准备下一轮迭代 (这里存在大量寄存器移动，测试并行度)
    mv  x3, x4         # F(i-2) = F(i-1)
    mv  x4, x6         # F(i-1) = F(i)
    
    # 指针与计数器更新
    addi x2, x2, 4     # 指针后移 4 字节
    addi x5, x5, 1     # i++
    
    # 循环分支
    blt  x5, x1, loop  # if i < 20, goto loop

    # --- 结束标志 ---
    # 触发你代码中定义的 EBREAK (机器码 0x00100073)
    ebreak
