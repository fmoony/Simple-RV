#include <stdint.h>
#include <stdbool.h>

// UART MMIO
#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_TX_IDLE  0x20

// CLINT timer
#define CLINT_MTIME_LO    (*((volatile uint32_t*)0x0200BFF8))

void uart_putchar(char c) {
    while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
    UART_DAT = c;
}

void print_str(const char* str) {
    while (*str != '\0') uart_putchar(*str++);
}

// 软件除法 (RV32I 无 M 扩展)
uint32_t __udivsi3(uint32_t n, uint32_t d) {
    if (d == 0) return 0;
    uint32_t q = 0, r = 0;
    for (int32_t i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (1u << i); }
    }
    return q;
}

uint32_t __umodsi3(uint32_t n, uint32_t d) {
    if (d == 0) return 0;
    uint32_t r = 0;
    for (int32_t i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) r -= d;
    }
    return r;
}

void print_dec(uint32_t val) {
    char buf[12];
    int32_t i = 10;
    buf[11] = '\0';
    if (val == 0) { print_str("0"); return; }
    while (val > 0) { buf[i--] = '0' + (char)__umodsi3(val, 10); val = __udivsi3(val, 10); }
    print_str(&buf[i + 1]);
}

// ==========================================
// Dhrystone 2.1 — adapted for bare-metal RV32I
// ==========================================

#define NUMBER_OF_RUNS 1000

typedef enum { Ident_1, Ident_2, Ident_3, Ident_4, Ident_5 } Enumeration;

typedef struct Rec_Type_ {
    struct Rec_Type_ *Ptr_Comp;
    int32_t           Discr;
    union {
        struct { Enumeration Enum_Comp; int32_t Int_Comp; } var_1;
        struct { Enumeration E_Comp_2;   char     Ch_2;   } var_2;
        struct { char        Ch_1;       char     Ch_2;   } var_3;
    } variant;
} Rec_Type;

// Globals
Rec_Type  Ptr_Glob;
Rec_Type  Next_Ptr_Glob;
int32_t   Int_Glob;
bool      Bool_Glob;
char      Ch_1_Glob, Ch_2_Glob;
int32_t   Arr_1_Glob[50];
int32_t   Arr_2_Glob[50][50];

// Forward declarations
void Proc_1(Rec_Type *Ptr_Val_Par);
void Proc_2(char *Int_Par_Ref);
void Proc_3(Rec_Type **Ptr_Val_Par);
void Proc_4(void);
void Proc_5(void);
void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par);
void Proc_7(int32_t Int_1_Par_Val, int32_t Int_2_Par_Val, int32_t *Int_Par_Ref);
void Proc_8(int32_t Arr_1_Par_Ref[50], int32_t Arr_2_Par_Ref[50][50], int32_t Int_1_Par_Val, int32_t Int_2_Par_Val);
void Proc_Init(void);

void Proc_1(Rec_Type *Ptr_Val_Par) {
    Rec_Type *Next_Record = Ptr_Val_Par->Ptr_Comp;
    *Ptr_Val_Par->Ptr_Comp = *Ptr_Glob.Ptr_Comp;
    Ptr_Val_Par->variant.var_1.Int_Comp = 5;
    Next_Record->variant.var_1.Int_Comp = Ptr_Val_Par->variant.var_1.Int_Comp;
    Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
    Proc_3(&Next_Record->Ptr_Comp);
    if (Next_Record->Discr == Ident_1) {
        Next_Record->variant.var_1.Int_Comp = 6;
        Proc_6(Ptr_Val_Par->variant.var_1.Enum_Comp, &Next_Record->variant.var_1.Enum_Comp);
        Next_Record->Ptr_Comp = Ptr_Glob.Ptr_Comp;
        Proc_7(Next_Record->variant.var_1.Int_Comp, 10, &Next_Record->variant.var_1.Int_Comp);
    } else {
        *Ptr_Val_Par = *Ptr_Val_Par->Ptr_Comp;
    }
}

void Proc_2(char *Int_Par_Ref) {
    if (Ch_1_Glob == *Int_Par_Ref + 'A') *Int_Par_Ref += 1;
    else *Int_Par_Ref = *Int_Par_Ref - 1;
}

void Proc_3(Rec_Type **Ptr_Val_Par) {
    if (Ptr_Glob.Ptr_Comp != 0) *Ptr_Val_Par = Ptr_Glob.Ptr_Comp;
    else Int_Glob = 100;
    Proc_7(10, Int_Glob, &Ptr_Glob.variant.var_1.Int_Comp);
}

void Proc_4(void) {
    Bool_Glob = (Ch_1_Glob == Ch_2_Glob) | (Bool_Glob == true);
    Ch_2_Glob = 'B';
}

void Proc_5(void) {
    Ch_1_Glob = 'A';
    Bool_Glob = false;
}

void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par) {
    *Enum_Ref_Par = Enum_Val_Par;
    if (Enum_Val_Par == Ident_3) *Enum_Ref_Par = Ident_4;
    switch (Enum_Val_Par) {
        case Ident_1: *Enum_Ref_Par = Ident_1; break;
        case Ident_2:
            if (Int_Glob > 100) *Enum_Ref_Par = Ident_1;
            else *Enum_Ref_Par = Ident_4;
            break;
        case Ident_3: *Enum_Ref_Par = Ident_2; break;
        case Ident_4: break;
        case Ident_5: *Enum_Ref_Par = Ident_3; break;
    }
}

void Proc_7(int32_t Int_1_Par_Val, int32_t Int_2_Par_Val, int32_t *Int_Par_Ref) {
    int32_t Loc = Int_1_Par_Val + 2;
    *Int_Par_Ref = Int_2_Par_Val + Loc;
}

void Proc_8(int32_t Arr_1_Par_Ref[50], int32_t Arr_2_Par_Ref[50][50], int32_t Int_1_Par_Val, int32_t Int_2_Par_Val) {
    int32_t Int_Loc = Int_1_Par_Val + 5;
    Arr_1_Par_Ref[Int_Loc] = Int_2_Par_Val;
    Arr_1_Par_Ref[Int_Loc + 1] = Arr_1_Par_Ref[Int_Loc];
    Arr_1_Par_Ref[Int_Loc + 30] = Int_Loc;
    for (int32_t i = Int_Loc; i <= Int_Loc + 1; i++)
        Arr_2_Par_Ref[Int_Loc][i] = Int_Loc;
    Arr_2_Par_Ref[Int_Loc][Int_Loc - 1] += 1;
    Arr_2_Par_Ref[Int_Loc + 20][Int_Loc] = Arr_1_Par_Ref[Int_Loc];
    Int_Glob = 5;
}

// ==========================================
// Init globals
// ==========================================
void Proc_Init(void) {
    Ptr_Glob.Ptr_Comp = &Next_Ptr_Glob;
    Ptr_Glob.Discr = Ident_1;
    Ptr_Glob.variant.var_1.Enum_Comp = Ident_3;
    Ptr_Glob.variant.var_1.Int_Comp = 40;

    Next_Ptr_Glob.Ptr_Comp = &Ptr_Glob;
    Next_Ptr_Glob.Discr = Ident_1;
    Next_Ptr_Glob.variant.var_1.Enum_Comp = Ident_2;
    Next_Ptr_Glob.variant.var_1.Int_Comp = 17;

    Int_Glob = 0;
    Bool_Glob = false;
    Ch_1_Glob = 'A';
    Ch_2_Glob = 'A';

    for (int32_t i = 0; i < 50; i++) {
        Arr_1_Glob[i] = 0;
        for (int32_t j = 0; j < 50; j++)
            Arr_2_Glob[i][j] = 0;
    }
}

// ==========================================
// Entry point
// ==========================================
void _start() {
    print_str("Dhrystone 2.1 Benchmark\n");
    print_str("Runs: ");
    print_dec(NUMBER_OF_RUNS);
    print_str("\n");

    uint32_t start_time = CLINT_MTIME_LO;
    Proc_Init();

    for (int32_t Run_Index = 1; Run_Index <= NUMBER_OF_RUNS; Run_Index++) {
        Proc_5();
        Proc_4();
        Int_Glob = 2;
        Bool_Glob = true;
        for (int32_t i = 1; i <= 7; i++) {
            Int_Glob *= 2;
            Proc_6(Ident_1, &Next_Ptr_Glob.variant.var_1.Enum_Comp);
            Proc_7(Int_Glob, 3, &Next_Ptr_Glob.variant.var_1.Int_Comp);
            Next_Ptr_Glob.Ptr_Comp = &Ptr_Glob;
            Proc_1(&Next_Ptr_Glob);
            Proc_7(10, Int_Glob, &Ptr_Glob.variant.var_1.Int_Comp);
        }
        Proc_2(&Ch_1_Glob);
        Proc_8(Arr_1_Glob, Arr_2_Glob, Int_Glob, 3);
    }

    uint32_t end_time = CLINT_MTIME_LO;
    uint32_t elapsed = end_time - start_time;

    print_str("Done.\nElapsed mtime ticks: ");
    print_dec(elapsed);
    print_str("\n");

    __asm__ volatile ("ebreak");
}
