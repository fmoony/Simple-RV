# Simple-RV-Plus

基于 C++17 实现的 **RISC-V RV32I 指令集模拟器**，采用 5 级双发射顺序流水线微架构，支持 64KB 物理内存、MMU（Sv32 分页）、CLINT 定时器中断、UART 串口外设及完整的同步异常处理机制。

## 特性

- **完整 RV32I 指令集** — 实现全部 38 条基础整数指令（含 CSR 指令）
- **5 级双发射流水线** — IF → ID → EX → MEM → WB，理论峰值 IPC 2.0
- **四级转发网络** — 周期内 / EX_MEM / MEM_WB / 寄存器文件，最大化指令吞吐
- **冒险处理** — RAW 转发、Load-Use 停顿、分支冲刷、结构冒险检测
- **MMU Sv32 分页** — 两级页表遍历 + 4MB 巨页支持 + TLB 缓存
- **中断异常** — ECALL/MRET 异常、定时器/软件/外部中断、非法指令/地址未对齐检测
- **外设** — UART 串口（跨平台键盘输入）、CLINT 定时器
- **裸机运行** — 10 个汇编测试 + 1 个贪吃蛇 C 游戏

## 快速开始

### 环境要求

- 支持 C++17 的编译器（GCC / Clang / MSVC）
- CMake ≥ 3.15
- RISC-V 交叉编译工具链（用于编译测试程序）：`riscv64-unknown-elf-gcc`

### 编译模拟器

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 运行

```bash
./sim-rv <program.bin>
```

示例：

```bash
# Fibonacci 计算
./sim-rv ../Resource/fib_test.bin

# 贪吃蛇游戏
./sim-rv ../Resource/CProgram/main.bin
```

## 项目结构

```
Simple-RV-Plus/
├── src/                    # 核心源代码
│   ├── main.cpp            # 入口，程序加载与异常捕获
│   ├── CPUCore.cpp         # 顶层 CPU 调度（流水线主循环、中断/陷阱处理）
│   ├── ExecutionEngine.cpp # ALU 执行、四级转发网络
│   ├── IssueUnit.cpp       # 指令译码、双发射决策、冒险检测
│   ├── MemorySystem.cpp    # 内存读写、MMIO 路由、MMU/TLB、CLINT
│   ├── PipelineRegisters.cpp # 流水线锁存器刷新/复位
│   └── RegisterFile.cpp    # 32 个 GPR + CSR 寄存器读写
├── include/                # 头文件
│   ├── common.h            # 类型定义、流水线结构体、CSR 地址宏
│   ├── CPUCore.h
│   ├── ExecutionEngine.h
│   ├── IssueUnit.h
│   ├── MemorySystem.h
│   ├── PipelineRegisters.h
│   ├── RegisterFile.h
│   └── SystemConfig.h      # 内存大小、初始 PC/SP/MTVEC 配置
├── Resource/               # 测试程序
│   ├── *.s                 # 汇编测试源码
│   ├── *.bin               # 编译后的二进制文件
│   ├── compile_asm.sh      # 汇编编译脚本
│   └── CProgram/           # C 语言测试程序
│       ├── main.c          # 贪吃蛇游戏
│       ├── compile_c.sh    # C 编译脚本
│       └── link.ld         # 链接脚本
├── CmakeLists.txt          # CMake 构建配置
└── README.md
```

## 流水线架构

```
┌─────────────────────────────────────────────────┐
│                   CPUCore                        │
│  tick() → WB → MEM → EX → ID → IF（反向执行）     │
├──────────┬──────────┬──────────┬────────────────┤
│ IssueUnit│ ExecEng  │ MemSys   │ RegisterFile   │
│ 译码/发射 │ ALU/转发  │ 内存/MMIO │ GPR + CSR      │
└──────────┴──────────┴──────────┴────────────────┘
```

### 冒险处理

| 冒险类型 | 检测阶段 | 处理方法 |
|---------|---------|---------|
| RAW（ALU→ALU） | EX | 4 级转发 |
| RAW（Load→Use） | ID | 1 周期停顿 + 气泡插入 |
| 结构冒险（双访存） | ID | 降级为单发射 |
| 控制冒险（分支） | EX | 冲刷 IF_ID 和 ID_EX |
| 控制冒险（ECALL/MRET） | EX | 清空全部流水线 |

## 配置

通过 `SystemConfig` 结构体配置硬件参数：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| ram_size | 64KB | 物理内存大小 |
| pc_init | 0x00000000 | 程序入口地址 |
| sp_init | 0xFFF0 | 栈指针初始值 |
| mtvec_init | 0x00003000 | 陷阱向量基址 |

## 测试程序

| 程序 | 测试目标 |
|------|---------|
| `echo` | UART 键盘回显 |
| `hello_uart` | Hello World 字符串输出 |
| `fib_test` | Fibonacci 数列计算与 RAW 转发 |
| `pipeline_stress` | 流水线综合压力（RAW/Load-Use/分支/双发射） |
| `isa_complete_test` | ISA 完整性（LUI/AUIPC/移位/JAL/JALR） |
| `test_trap` | ECALL → ISR → MRET 异常处理 |
| `test_arit_uart` | 立即数运算 + UART 打印 |
| `test_binary` | 分支判断 |
| `test_cycle` | 循环周期计数验证 |
| `test_mem` | 内存读写一致性 |
| `main` (C) | 贪吃蛇游戏（系统级功能验证） |

## 构建测试程序

```bash
cd Resource

# 编译汇编测试
./compile_asm.sh echo.s
./compile_asm.sh fib_test.s

# 编译 C 程序
cd CProgram
./compile_c.sh main.c
```

## 局限与待改进

- 无分支预测器（假设不命中 + 命中冲刷）
- 固定 64KB 内存，无缓存层次
- 支持 M-Mode，无 S-Mode/U-Mode
- CSR 中断委托机制未完全实现

## 许可

本项目仅供学习与研究使用。
