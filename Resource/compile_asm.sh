#!/bin/bash
# Usage: ./compile_asm.sh test.s [rv32i|rv32i_zicsr]
FILE_NAME="${1%.*}"
ARCH="${2:-rv32i_zicsr}"

echo "Compiling $1 with arch=$ARCH..."
riscv64-unknown-elf-gcc -march=$ARCH -mabi=ilp32 -c $1 -o ${FILE_NAME}.o
riscv64-unknown-elf-ld -m elf32lriscv --section-start .text=0x0 ${FILE_NAME}.o -o ${FILE_NAME}.elf
riscv64-unknown-elf-objcopy -O binary ${FILE_NAME}.elf ${FILE_NAME}.bin

echo "Done! Generated ${FILE_NAME}.bin"
