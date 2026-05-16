#!/bin/bash
# Usage: ./compile_c.sh
# Compiles the snake game C program for Simple-RV
# Requires riscv64-unknown-elf-gcc with zicsr support

echo "Compiling main.c..."
riscv64-unknown-elf-gcc -O1 -march=rv32i_zicsr -mabi=ilp32 \
    -nostartfiles -nostdlib -fno-toplevel-reorder \
    -T link.ld main.c -o main.elf

riscv64-unknown-elf-objcopy -O binary main.elf main.bin

echo "Done! Generated main.bin"
