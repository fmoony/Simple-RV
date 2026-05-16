/* link.ld - Simple-RV 裸机链接器脚本 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    /* 1. 一切从 0x00000000 开始 */
    . = 0x00000000;

    /* 2. 存放可执行代码 */
    .text : {
        *(.text)
        *(.text.*)
    }

    /* 3. 存放只读数据 (比如字符串常量) */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* 4. 存放已初始化的全局变量 */
    .data : {
        *(.data)
        *(.data.*)
        *(.sdata)
    }

    /* 5. 存放未初始化的全局变量 */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(COMMON)
    }

    /* 丢弃不需要的调试段，减小体积 */
    /DISCARD/ : {
        *(.eh_frame)
    }
}