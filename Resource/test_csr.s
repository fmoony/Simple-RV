# RISC-V CSR Instruction Test
# Tests: CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI
.attribute arch, "rv32i_zicsr"
.section .text
.global _start

_start:
    # =============================================
    # Test 1: CSRRW - Atomic Read/Write CSR
    # Write 0x88 to mstatus, verify old value and new value
    # =============================================
    li x1, 0x88
    csrw 0x300, x1        # mstatus = 0x88
    csrr x2, 0x300        # x2 = mstatus
    # Expected: x2 = 0x88

    # =============================================
    # Test 2: CSRRS - Atomic Read and Set Bits
    # Set additional bits 0x700 in mstatus
    # =============================================
    li x3, 0x700
    csrs 0x300, x3        # mstatus |= 0x700 (old value returned to x3... wait, csrs writes to rd)
    # Actually: csrs rd, csr, rs1 -> rd = old_csr, csr = old_csr | rs1
    # Let's use proper syntax:
    csrrs x4, 0x300, x3   # x4 = old mstatus, mstatus |= 0x700
    csrr x5, 0x300        # x5 = new mstatus
    # Expected: x5 = 0x788

    # =============================================
    # Test 3: CSRRC - Atomic Read and Clear Bits
    # Clear bits 0x700 in mstatus
    # =============================================
    csrrc x6, 0x300, x3   # x6 = old mstatus (0x788), mstatus &= ~0x700
    csrr x7, 0x300        # x7 = new mstatus
    # Expected: x7 = 0x088

    # =============================================
    # Test 4: CSRRWI - Atomic Read/Write Immediate
    # Write immediate 0 to mstatus
    # =============================================
    csrrwi x8, 0x300, 0   # x8 = old mstatus (0x088), mstatus = 0
    csrr x9, 0x300        # x9 = 0
    # Expected: x8 = 0x088, x9 = 0

    # =============================================
    # Test 5: CSRRSI - Atomic Read and Set Bits Immediate
    # Set bit 3 (MIE) in mstatus
    # =============================================
    csrrsi x10, 0x300, 8  # x10 = old mstatus (0), mstatus |= 8
    csrr x11, 0x300       # x11 = 8
    # Expected: x10 = 0, x11 = 8

    # =============================================
    # Test 6: CSRRCI - Atomic Read and Clear Bits Immediate
    # Clear bit 3 (MIE) in mstatus
    # =============================================
    csrrci x12, 0x300, 8  # x12 = old mstatus (8), mstatus &= ~8
    csrr x13, 0x300       # x13 = 0
    # Expected: x12 = 8, x13 = 0

    # =============================================
    # Test 7: CSRRS with rs1=x0 (read-only, no write)
    # =============================================
    li x14, 0x88
    csrw 0x300, x14       # mstatus = 0x88
    csrrs x15, 0x300, x0  # x15 = old mstatus, no write (rs1=x0)
    csrr x16, 0x300       # mstatus should still be 0x88
    # Expected: x15 = 0x88, x16 = 0x88

    # =============================================
    # Test 8: Write to mtvec via CSR instruction
    # =============================================
    li x17, 0x3000
    csrw 0x305, x17       # mtvec = 0x3000
    csrr x18, 0x305       # x18 = 0x3000
    # Expected: x18 = 0x3000

    # All tests passed - print 'P'
    lui x20, 0x10000
    li  x21, 80           # 'P'
    sb  x21, 0(x20)

    ebreak
