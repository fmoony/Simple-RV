#pragma once
#include "common.h"

// MMU translation result
struct TranslationResult {
    bool fault = false;
    Addr paddr = 0;
    Word cause = 0;  // mcause code for page fault
};

class MemorySystem
{
private:
    std::vector<Byte> memory;   // 物理内存
    uint64_t mtime = 0;         // CLINT 64 位定时器（每周期递增）
    uint64_t mtimecmp = 0;      // CLINT 64 位定时器比较值

    // 简易 TLB（用于 MMU）
    static const int TLB_SIZE = 8;
    struct TLBEntry {
        Word vpn = 0;           // Virtual Page Number
        Word ppn = 0;           // Physical Page Number
        Byte flags = 0;         // R=1, W=2, X=4, U=8
        bool valid = false;
    };
    std::array<TLBEntry, TLB_SIZE> tlb;
    uint32_t tlb_replace_idx = 0;

    void tlb_insert(Word vpn, Word ppn, Byte flags);
    bool tlb_lookup(Word vpn, TranslationResult& result, bool is_write, bool is_fetch) const;

public:
    MemorySystem() : memory(MEMORY_SIZE, 0) {}

    // 内存访问
    Word read(Addr addr, Byte n) const;
    uint64_t read_double_instr(Addr pc) const;
    void write(Addr addr, Byte n, Word data);

    // 程序加载
    void load_program(const std::string& filename);

    // CLINT 定时器
    void increment_timer();
    bool timer_interrupt_pending() const;

    // MMU：虚拟地址转物理地址
    // satp：当前 satp CSR 寄存器的值
    // privilege：当前特权级（用于 U 位检查）
    TranslationResult translate(Addr vaddr, bool is_write, bool is_fetch,
                                Word satp, Byte privilege) const;
    void tlb_flush();
};
