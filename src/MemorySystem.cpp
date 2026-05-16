#include "../include/MemorySystem.h"
#include <cstring>

// =========================================================================
// 跨平台非阻塞键盘输入头文件
// =========================================================================
#if defined(_WIN32) || defined(_WIN64)
    #include <conio.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>
#endif

// =========================================================================
// 全局键盘缓冲区
// =========================================================================
static char buffered_char = 0;
static bool has_buffered_char = false;

// =========================================================================
// 跨平台非阻塞键盘检测
// =========================================================================
void check_host_keyboard() {
    if (has_buffered_char) return;

#if defined(_WIN32) || defined(_WIN64)
    if (_kbhit()) {
        buffered_char = static_cast<char>(_getch());
        has_buffered_char = true;
    }
#else
    struct termios oldt, newt;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    char ch;
    int bytes_read = read(STDIN_FILENO, &ch, 1);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (bytes_read > 0) {
        buffered_char = ch;
        has_buffered_char = true;
    }
#endif
}

// =========================================================================
// 内存读取（含 MMIO 路由）
// =========================================================================
Word MemorySystem::read(Addr addr, Byte n) const
{
    check_host_keyboard();

    // --- CLINT MMIO 读取 ---
    if (addr >= CLINT_BASE && addr < CLINT_BASE + 0x10000) {
        if (addr == CLINT_MTIME && n == 4) {
            return static_cast<Word>(mtime & 0xFFFFFFFF);
        }
        if (addr == CLINT_MTIME + 4 && n == 4) {
            return static_cast<Word>((mtime >> 32) & 0xFFFFFFFF);
        }
        if (addr == CLINT_MTIMECMP && n == 4) {
            return static_cast<Word>(mtimecmp & 0xFFFFFFFF);
        }
        if (addr == CLINT_MTIMECMP + 4 && n == 4) {
            return static_cast<Word>((mtimecmp >> 32) & 0xFFFFFFFF);
        }
        return 0;
    }

    // --- UART MMIO 读取 ---
    if (addr == UART_LSR_ADDR) {
        Word status = 0x20;  // THR empty (always ready to transmit)
        if (has_buffered_char) {
            status |= 0x01;  // 数据就绪
        }
        return status;
    }

    if (addr == UART_TX_ADDR) {
        if (has_buffered_char) {
            Word data = static_cast<Word>(buffered_char);
            has_buffered_char = false;
            return data;
        }
        return 0x00;
    }

    // --- 物理内存访问 ---
    if (addr + n > MEMORY_SIZE) {
        throw std::runtime_error("Memory read access out of bounds");
    }

    Word data = 0;
    for (int i = 0; i < n; ++i) {
        data |= static_cast<Word>(memory[addr + i]) << (8 * i);
    }
    return data;
}

// =========================================================================
// 双指令取指（用于双发射）
// =========================================================================
uint64_t MemorySystem::read_double_instr(Addr pc) const
{
    if (pc + 8 > MEMORY_SIZE) {
        throw std::runtime_error("Instruction fetch out of bounds");
    }

    uint64_t data = 0;
    std::memcpy(&data, &memory[pc], 8);
    return data;
}

// =========================================================================
// 内存写入（含 MMIO 路由）
// =========================================================================
void MemorySystem::write(Addr addr, Byte n, Word data)
{
    // --- CLINT MMIO 写入 ---
    if (addr >= CLINT_BASE && addr < CLINT_BASE + 0x10000) {
        if (addr == CLINT_MTIMECMP && n == 4) {
            mtimecmp = (mtimecmp & 0xFFFFFFFF00000000ULL) | data;
            return;
        }
        if (addr == CLINT_MTIMECMP + 4 && n == 4) {
            mtimecmp = (mtimecmp & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
            return;
        }
        // mtime 只读（由硬件递增）
        return;
    }

    // --- UART MMIO 写入 ---
    if (addr == UART_TX_ADDR) {
        std::cout << static_cast<char>(data) << std::flush;
        return;
    }

    // --- 物理内存访问 ---
    if (addr + n > MEMORY_SIZE) {
        throw std::runtime_error("Memory write access out of bounds");
    }

    for (int i = 0; i < n; ++i) {
        memory[addr + i] = static_cast<Byte>((data >> (8 * i)) & 0xFF);
    }
}

// =========================================================================
// 程序加载
// =========================================================================
void MemorySystem::load_program(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open program file");
    }

    // 读取整个文件（最大不超过物理内存大小）
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (file_size > memory.size()) file_size = memory.size();
    file.read(reinterpret_cast<char*>(&memory[0]), file_size);
    file.close();
}

// =========================================================================
// CLINT 定时器
// =========================================================================
void MemorySystem::increment_timer()
{
    mtime++;
}

bool MemorySystem::timer_interrupt_pending() const
{
    return (mtime >= mtimecmp && mtimecmp != 0);
}

// =========================================================================
// MMU：Sv32 页表遍历
// =========================================================================
void MemorySystem::tlb_insert(Word vpn, Word ppn, Byte flags)
{
    tlb[tlb_replace_idx].vpn = vpn;
    tlb[tlb_replace_idx].ppn = ppn;
    tlb[tlb_replace_idx].flags = flags;
    tlb[tlb_replace_idx].valid = true;
    tlb_replace_idx = (tlb_replace_idx + 1) % TLB_SIZE;
}

bool MemorySystem::tlb_lookup(Word vpn, TranslationResult& result, bool is_write, bool is_fetch) const
{
    for (int i = 0; i < TLB_SIZE; ++i) {
        if (tlb[i].valid && tlb[i].vpn == vpn) {
            Byte flags = tlb[i].flags;

            // 权限检查
            if (is_fetch && !(flags & 0x4)) {  // X 位
                result.fault = true;
                result.cause = MCAUSE_INST_PAGE_FAULT;
                return false;
            }
            if (is_write && !(flags & 0x2)) {   // W 位
                result.fault = true;
                result.cause = MCAUSE_STORE_PAGE_FAULT;
                return false;
            }
            if (!is_write && !is_fetch && !(flags & 0x1)) { // R 位
                result.fault = true;
                result.cause = MCAUSE_LOAD_PAGE_FAULT;
                return false;
            }

            result.paddr = (tlb[i].ppn << 12) | (vpn & 0xFFF);
            result.fault = false;
            return true;
        }
    }
    return false;  // TLB 未命中
}

TranslationResult MemorySystem::translate(Addr vaddr, bool is_write, bool is_fetch,
                                          Word satp, Byte privilege) const
{
    TranslationResult result;
    result.paddr = vaddr;  // 默认：直接映射
    result.fault = false;

    // M-mode 始终使用裸机地址（物理地址 = 虚拟地址）
    if (privilege == PRV_M) {
        return result;
    }

    // 检查 SATP 模式：bit 31 = 0 => 裸机（无地址转换）
    Word mode = (satp >> 31) & 0x1;
    if (mode == 0) {
        return result;  // 裸机模式：恒等映射
    }

    // Sv32 模式：两级页表遍历
    Word vpn1 = (vaddr >> 22) & 0x3FF;  // VPN[1] = VA[31:22]
    Word vpn0 = (vaddr >> 12) & 0x3FF;  // VPN[0] = VA[21:12]
    Word offset = vaddr & 0xFFF;        // 页内偏移

    // TLB 查找
    Word vpn_full = (vpn1 << 10) | vpn0;
    if (tlb_lookup(vpn_full, result, is_write, is_fetch)) {
        return result;
    }

    // 根页表：PPN 取自 satp[21:0]，左移 12 位
    Addr root_ppn = satp & 0x003FFFFF;  // 22 位 PPN
    Addr root_addr = root_ppn << 12;

    // 读取一级 PTE
    if (root_addr + vpn1 * 4 + 4 > MEMORY_SIZE) {
        result.fault = true;
        result.cause = is_fetch ? MCAUSE_INST_PAGE_FAULT :
                       (is_write ? MCAUSE_STORE_PAGE_FAULT : MCAUSE_LOAD_PAGE_FAULT);
        return result;
    }

    Word pte1_raw = 0;
    for (int i = 0; i < 4; ++i) {
        pte1_raw |= static_cast<Word>(memory[root_addr + vpn1 * 4 + i]) << (8 * i);
    }

    bool pte1_v = pte1_raw & 0x01;
    bool pte1_r = pte1_raw & 0x02;
    bool pte1_w = pte1_raw & 0x04;
    bool pte1_x = pte1_raw & 0x08;
    bool pte1_u = pte1_raw & 0x10;
    Word pte1_ppn = (pte1_raw >> 10) & 0x003FFFFF;

    if (!pte1_v) {
        result.fault = true;
        result.cause = is_fetch ? MCAUSE_INST_PAGE_FAULT :
                       (is_write ? MCAUSE_STORE_PAGE_FAULT : MCAUSE_LOAD_PAGE_FAULT);
        return result;
    }

    // 检查一级是否为叶子节点（巨页：R 或 X 置位）
    bool is_megapage = pte1_r || pte1_x;
    if (is_megapage) {
        // 4 MB 巨页：PPN 取自一级 PTE，偏移取自 VA[21:0]
        Byte flags = (pte1_r ? 0x1 : 0) | (pte1_w ? 0x2 : 0) | (pte1_x ? 0x4 : 0);
        Addr paddr = (pte1_ppn << 12) | (vaddr & 0x003FFFFF);  // 22-bit offset

        // 权限检查
        if (is_fetch && !pte1_x) {
            result.fault = true;
            result.cause = MCAUSE_INST_PAGE_FAULT;
            return result;
        }
        if (is_write && !pte1_w) {
            result.fault = true;
            result.cause = MCAUSE_STORE_PAGE_FAULT;
            return result;
        }
        if (!is_write && !is_fetch && !pte1_r) {
            result.fault = true;
            result.cause = MCAUSE_LOAD_PAGE_FAULT;
            return result;
        }
        // U 位检查：U 模式仅能访问 U=1 的页面
        if (privilege == PRV_U && !pte1_u) {
            result.fault = true;
            result.cause = is_fetch ? MCAUSE_INST_PAGE_FAULT :
                           (is_write ? MCAUSE_STORE_PAGE_FAULT : MCAUSE_LOAD_PAGE_FAULT);
            return result;
        }

        // 插入 TLB
        const_cast<MemorySystem*>(this)->tlb_insert(vpn_full, pte1_ppn, flags);

        result.paddr = paddr;
        result.fault = false;
        return result;
    }

    // 零级 PTE（指向下一级）
    Addr l0_addr = (pte1_ppn << 12) + vpn0 * 4;
    if (l0_addr + 4 > MEMORY_SIZE) {
        result.fault = true;
        result.cause = is_fetch ? MCAUSE_INST_PAGE_FAULT :
                       (is_write ? MCAUSE_STORE_PAGE_FAULT : MCAUSE_LOAD_PAGE_FAULT);
        return result;
    }

    Word pte0_raw = 0;
    for (int i = 0; i < 4; ++i) {
        pte0_raw |= static_cast<Word>(memory[l0_addr + i]) << (8 * i);
    }

    bool pte0_v = pte0_raw & 0x01;
    bool pte0_r = pte0_raw & 0x02;
    bool pte0_w = pte0_raw & 0x04;
    bool pte0_x = pte0_raw & 0x08;
    bool pte0_u = pte0_raw & 0x10;
    Word pte0_ppn = (pte0_raw >> 10) & 0x003FFFFF;

    if (!pte0_v) {
        result.fault = true;
        result.cause = is_fetch ? MCAUSE_INST_PAGE_FAULT :
                       (is_write ? MCAUSE_STORE_PAGE_FAULT : MCAUSE_LOAD_PAGE_FAULT);
        return result;
    }

    // Permission check for 4KB page
    if (is_fetch && !pte0_x) {
        result.fault = true;
        result.cause = MCAUSE_INST_PAGE_FAULT;
        return result;
    }
    if (is_write && !pte0_w) {
        result.fault = true;
        result.cause = MCAUSE_STORE_PAGE_FAULT;
        return result;
    }
    if (!is_write && !is_fetch && !pte0_r) {
        result.fault = true;
        result.cause = MCAUSE_LOAD_PAGE_FAULT;
        return result;
    }
    // U 位检查：U 模式仅能访问 U=1 的页面
    if (privilege == PRV_U && !pte0_u) {
        result.fault = true;
        result.cause = is_fetch ? MCAUSE_INST_PAGE_FAULT :
                       (is_write ? MCAUSE_STORE_PAGE_FAULT : MCAUSE_LOAD_PAGE_FAULT);
        return result;
    }

    // Insert into TLB
    Byte flags = (pte0_r ? 0x1 : 0) | (pte0_w ? 0x2 : 0) | (pte0_x ? 0x4 : 0);
    const_cast<MemorySystem*>(this)->tlb_insert(vpn_full, pte0_ppn, flags);

    result.paddr = (pte0_ppn << 12) | offset;
    result.fault = false;
    return result;
}

void MemorySystem::tlb_flush()
{
    for (int i = 0; i < TLB_SIZE; ++i) {
        tlb[i].valid = false;
    }
    tlb_replace_idx = 0;
}
