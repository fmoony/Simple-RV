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
    std::vector<Byte> memory;   // Physical memory
    uint64_t mtime = 0;         // CLINT 64-bit timer (increments each cycle)
    uint64_t mtimecmp = 0;      // CLINT 64-bit timer compare

    // Simple TLB for MMU (Phase 3)
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

    // Memory access
    Word read(Addr addr, Byte n) const;
    uint64_t read_double_instr(Addr pc) const;
    void write(Addr addr, Byte n, Word data);

    // Program loading
    void load_program(const std::string& filename);

    // CLINT timer
    void increment_timer();
    bool timer_interrupt_pending() const;

    // MMU (Phase 3): translate virtual to physical address
    // satp: current satp CSR value from RegisterFile
    TranslationResult translate(Addr vaddr, bool is_write, bool is_fetch, Word satp) const;
    void tlb_flush();
};
