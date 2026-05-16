#!/bin/bash
# 用法: ./compile_c.sh
# 编译贪吃蛇 C 程序，用于 Simple-RV
# 需要 riscv64-unknown-elf-gcc（带 zicsr 支持）

echo "正在编译 main.c..."
riscv64-unknown-elf-gcc -O2 -march=rv32i_zicsr -mabi=ilp32 \
    -nostartfiles -nostdlib -fno-toplevel-reorder \
    -T link.ld main.c -o main.elf

riscv64-unknown-elf-objcopy -O binary main.elf main.bin

echo "完成！已生成 main.bin"
