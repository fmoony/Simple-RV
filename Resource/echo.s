# RISC-V 串口输入测试：极简复读机 (Echo)

.section .text
.globl _start

_start:
    lui x10, 0x10000       # x10 = 0x10000000 (UART 基地址)

wait_for_input:
    # 1. 读取 LSR 状态寄存器 (偏移为 5)
    lb  x11, 5(x10)        
    
    # 2. 检查最低位 (Data Ready) 是否为 1
    andi x12, x11, 1       
    beq  x12, x0, wait_for_input # 如果是 0，说明没按键，死循环继续等
    
    # 3. 发现按键！从 RX 寄存器 (偏移为 0) 读取输入的字符
    lb  x13, 0(x10)        
    
    # 4. 立刻把读到的字符原样写回 TX 寄存器 (偏移为 0) 打印出来！
    sb  x13, 0(x10)        
    
    # 5. 检查是不是按了 'q' (ASCII 码 113)。如果是，就退出程序
    addi x14, x0, 113
    beq  x13, x14, end_program
    
    # 6. 跳回开头，继续等待下一个按键
    jal x0, wait_for_input 

end_program:
    ebreak
