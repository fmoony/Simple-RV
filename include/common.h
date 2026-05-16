#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <array>
#include <iostream>
#include <fstream>

// RV32I ���ĳ���
const uint32_t MEMORY_SIZE = 64 * 1024; // 64KB
const uint32_t REG_COUNT = 32;          // x0-x31

using Word = uint32_t;
using Addr = uint32_t;

using HalfWord = uint16_t;
using Byte = uint8_t;

// --- UART MMIO 地址映射 ---
const Addr UART_TX_ADDR = 0x10000000; // 发送数据寄存器 (Write) / 接收数据寄存器 (Read)
const Addr UART_LSR_ADDR = 0x10000005; // 线路状态寄存器 (Line Status Register)

// --- CLINT MMIO 地址映射 (RISC-V CLINT 标准) ---
const Addr CLINT_BASE      = 0x02000000;
const Addr CLINT_MTIMECMP  = 0x02004000;  // hart 0 mtimecmp (lower 32)
const Addr CLINT_MTIME     = 0x0200BFF8;  // mtime lower 32 bits


// --- �������ݽṹ ---
// �洢ָ�������ľ�̬��Ϣ��������ˮ�߸��׶δ���
struct DecodedData 
{
    Byte op = 0;          // �����루��7λ�� 
    Byte rs1 = 0;         // Դ�Ĵ���1 
    Byte rs2 = 0;         // Դ�Ĵ���2 
    Byte rd = 0;          // Ŀ��Ĵ��� 
	Byte funct3 = 0;      // funct3 �ֶ� (instr[14:12])
	Byte funct7 = 0;      // funct7 �ֶ� (instr[31:25])
    int32_t imm = 0;         // ������չ��������� 

    // ָ�����Ա�־λ 
    bool uses_rs1 = false;   // �Ƿ��ȡrs1 
    bool uses_rs2 = false;   // �Ƿ��ȡrs2 
    bool regWrite = false;   // �Ƿ���Ҫд�ؼĴ��� 
    bool is_memory = false;  // �Ƿ�Ϊ�ô�ָ�� (Load/Store) 
    bool is_branch = false;  // �Ƿ�Ϊ��תָ�� 
    bool is_ecall = false;
    bool is_mret = false;
    bool is_csr = false;        // CSR 指令 (op=0x73, funct3!=0)
    bool is_illegal = false;    // 非法指令
    HalfWord csr_addr = 0;      // 12-bit CSR 地址 (instr[31:20])
};

// --- ��ˮ�߲�λ ---
// �������ݵ�Ԫ��֧��˫����ʱ�ɶ�ָ��Ĵ洢 
struct PipelineSlot 
{
    Word instr = 0;      // ԭʼ32λָ���� 
    DecodedData d;           // ��������ϸ��� 
    Word result = 0;     // ִ�н׶μ�����Ľ�� 
    Byte rd = 0;          // Ŀ��Ĵ������� 
    bool valid = false;      // ��λ��Ч�Ա�־��˫������ܽ�һ����Ч��
    bool regWrite = false;   // дʹ�ܿ����ź� 

    // 专���字段���专用于控制���跳转
    Addr jump_target = 0;

    // CSR 写���值 (EX 阶段计算，CPUCore::execute 提���)
    Word csr_write_val = 0;

    std::string getDisasm(const Addr pc) const {
        if (!valid) return "--- [Empty] ---";
        char buf[64];
        // ��ӡʮ������ PC �ͻ�����
        snprintf(buf, sizeof(buf), "0x%04X: [%08X]", pc, instr);
        return std::string(buf);
    }
};

// --- ��ˮ�߽׶λ���Ĵ��� (Pipeline Latches) ---

// IF -> ID: ����ȡ�ص�˫ָ���ǰPC 
struct IF_ID_Buffer 
{
    PipelineSlot slots[2];   // �洢ȡָ�׶ε�����ָ�� 
    Addr pc = 0;         // ȡָʱ��PC��ַ 
    bool flush = false;      // ��ˢ��־λ 
};

// ID -> EX: ���ݲ��������ô�����ź� 
struct ID_EX_Buffer 
{
    PipelineSlot slots[2];
    Addr pc = 0;
    bool memRead[2] = { false, false };  // ����ָ����ԵĶ�ʹ��
    bool memWrite[2] = { false, false }; // ����ָ����Ե�дʹ��
};

// EX -> MEM: ���ݼ�������ô��ַ [cite: 28, 31]
struct EX_MEM_Buffer 
{
    PipelineSlot slots[2];
    Addr pc = 0;
    Addr mem_addr[2] = { 0, 0 };     // �ô��Ŀ���ַ 
    Addr mem_data[2] = { 0, 0 };     // ��д���ڴ������ 
    bool memRead[2] = { false, false };
    bool memWrite[2] = { false, false };
};

// MEM -> WB: ��������д������ 
struct MEM_WB_Buffer 
{
    PipelineSlot slots[2];
    bool regWrite[2] = { false, false }; // д�ؿ����ź� 
};

// ==========================================
// RISC-V ����ģʽ (Machine Mode) CSR ��ַӳ���
// ==========================================
#define CSR_MVENDORID  0xF11  // ��Ӧ�� ID
#define CSR_MARCHID    0xF12  // �ܹ� ID
#define CSR_MIMPID     0xF13  // ʵ�� ID
#define CSR_MHARTID    0xF14  // Ӳ���߳� ID

#define CSR_MSTATUS    0x300  // ����ģʽ״̬�Ĵ���
#define CSR_MISA       0x301  // ����ģʽָ���չ
#define CSR_MEDELEG    0x302  // ����ģʽ�쳣ί��
#define CSR_MIDELEG    0x303  // ����ģʽ�ж�ί��
#define CSR_MIE        0x304  // ����ģʽ�ж�ʹ��
#define CSR_MTVEC      0x305  // ����ģʽ�쳣������ַ (��ղ��õ��� 0x305��)

#define CSR_MSCRATCH   0x340  // ����ģʽ��ʱ�Ĵ���
#define CSR_MEPC       0x341  // ����ģʽ�쳣 PC (���汻�жϵ� PC)
#define CSR_MCAUSE     0x342  // ����ģʽ�쳣ԭ��
#define CSR_SATP       0x180  // Supervisor Address Translation and Protection (Sv32)

#define CSR_MTVAL      0x343  // ����ģʽ�쳣ֵ
#define CSR_MIP        0x344

// --- MSTATUS bit masks ---
#define MSTATUS_MIE   0x00000008  // bit 3: Machine Interrupt Enable
#define MSTATUS_MPIE  0x00000080  // bit 7: Machine Previous Interrupt Enable

// --- MIE / MIP bit masks ---
#define MIE_MSIE  0x00000008  // bit 3: Software
#define MIE_MTIE  0x00000080  // bit 7: Timer
#define MIE_MEIE  0x00000800  // bit 11: External
#define MIP_MSIP  0x00000008
#define MIP_MTIP  0x00000080
#define MIP_MEIP  0x00000800

// --- MCAUSE exception/interrupt codes ---
#define MCAUSE_ILLEGAL          2
#define MCAUSE_BREAKPOINT       3
#define MCAUSE_LOAD_MISALIGNED  4
#define MCAUSE_LOAD_FAULT       5
#define MCAUSE_STORE_MISALIGNED 6
#define MCAUSE_STORE_FAULT      7
#define MCAUSE_ECALL_M         11
#define MCAUSE_INST_PAGE_FAULT 12
#define MCAUSE_LOAD_PAGE_FAULT 13
#define MCAUSE_STORE_PAGE_FAULT 15
#define MCAUSE_TIMER_INT       0x80000007  // bit31=1 + cause 7
#define MCAUSE_MSI_INT         0x80000003  // bit31=1 + cause 3
#define MCAUSE_MEI_INT         0x8000000B  // bit31=1 + cause 11

// --- MISA ---
#define MISA_RV32I  0x40000104  // RV32I base ISA  // ����ģʽ�жϹ���