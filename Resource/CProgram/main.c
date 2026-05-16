#include <stdint.h>
#include <stdbool.h>

// ==========================================
// 1. Hardware MMIO defines
// ==========================================
#define UART_DAT (*((volatile uint8_t*)0x10000000))
#define UART_LSR (*((volatile uint8_t*)0x10000005))
#define UART_LSR_RX_READY 0x01
#define UART_LSR_TX_IDLE  0x20

// CLINT timer registers (MMIO)
#define CLINT_MTIMECMP_LO (*((volatile uint32_t*)0x02004000))
#define CLINT_MTIMECMP_HI (*((volatile uint32_t*)0x02004004))
#define CLINT_MTIME_LO    (*((volatile uint32_t*)0x0200BFF8))

#define WIDTH  20
#define HEIGHT 10
#define MAX_LEN 100

// ==========================================
// 2. Global state
// ==========================================
int snake_x[MAX_LEN];
int snake_y[MAX_LEN];
int snake_len;
int dir_x, dir_y;
int apple_x, apple_y;
bool game_over;
int score;
uint32_t rand_state = 123456789;

char key_queue[16];
int q_head = 0;
int q_tail = 0;
char last_pushed_key = '\0';

// Timer tick counter (incremented by ISR)
volatile uint32_t timer_ticks = 0;

// ISR prototype (defined after _start)
void timer_isr(void);

// ==========================================
// 3. Forward declarations
// ==========================================
void uart_putchar(char c);
void print_str(const char* str);
bool uart_kbhit(void);
char uart_getchar(void);
void clear_screen(void);
uint32_t simple_rand(void);
void delay(uint32_t cycles);
void init_game(void);
void draw(void);
void logic(void);
void push_key(char k);
char pop_key(void);

uint32_t __udivsi3(uint32_t n, uint32_t d);
uint32_t __umodsi3(uint32_t n, uint32_t d);
int32_t __divsi3(int32_t n, int32_t d);
int32_t __modsi3(int32_t n, int32_t d);
int32_t __mulsi3(int32_t a, int32_t b);

// ==========================================
// 4. Entry point (must be FIRST function)
// ==========================================
void _start() {
    // Set up timer ISR as trap handler
    uint32_t isr_addr = (uint32_t)&timer_isr;
    __asm__ volatile ("csrw 0x305, %0" :: "r"(isr_addr));  // mtvec = &timer_isr

    // Enable timer interrupt (MIE.MTIE = bit 7, value 0x80 > 5-bit imm, use csrs)
    uint32_t mtie_val = 0x80;
    __asm__ volatile ("csrs 0x304, %0" :: "r"(mtie_val));  // MIE |= MTIE

    // Enable global interrupts (MSTATUS.MIE = bit 3)
    __asm__ volatile ("csrsi 0x300, 0x08");  // MSTATUS |= MIE

    init_game();

    while (!game_over) {
        draw();
        logic();

        // Timer-based delay (adjust this value to control game speed)
        delay(500000);
    }

    clear_screen();
    print_str("\n\n");
    print_str("   GAME OVER!\n");
    print_str("   System Halting...\n");

    __asm__ volatile ("ebreak");
}

// ==========================================
// 5. Timer interrupt handler (ISR)
//    __attribute__((interrupt)) generates
//    automatic register save/restore + mret
// ==========================================
__attribute__((interrupt("machine")))
void timer_isr(void) {
    timer_ticks++;
    // Clear timer interrupt by setting mtimecmp to max
    CLINT_MTIMECMP_LO = 0xFFFFFFFF;
    CLINT_MTIMECMP_HI = 0xFFFFFFFF;
}

// ==========================================
// 6. I/O and utilities
// ==========================================
void push_key(char k) {
    if (k == last_pushed_key) return;
    int next = (q_tail + 1) % 16;
    if (next != q_head) {
        key_queue[q_tail] = k;
        q_tail = next;
        last_pushed_key = k;
    }
}

char pop_key() {
    if (q_head == q_tail) return '\0';
    char k = key_queue[q_head];
    q_head = (q_head + 1) % 16;
    if (q_head == q_tail) last_pushed_key = '\0';
    return k;
}

void uart_putchar(char c) {
    while ((UART_LSR & UART_LSR_TX_IDLE) == 0);
    UART_DAT = c;
}

void print_str(const char* str) {
    while (*str != '\0') {
        uart_putchar(*str++);
    }
}

bool uart_kbhit() {
    return (UART_LSR & UART_LSR_RX_READY) != 0;
}

char uart_getchar() {
    return UART_DAT;
}

void clear_screen() {
    print_str("\033[H\033[2J");
}

uint32_t simple_rand() {
    rand_state ^= rand_state << 13;
    rand_state ^= rand_state >> 17;
    rand_state ^= rand_state << 5;
    return rand_state;
}

// ==========================================
// 7. Timer-based delay (replaces busy-wait)
//    Sets mtimecmp = mtime + cycles, then
//    waits for ISR to increment timer_ticks
// ==========================================
void delay(uint32_t cycles) {
    uint32_t now = CLINT_MTIME_LO;
    CLINT_MTIMECMP_LO = now + cycles;
    CLINT_MTIMECMP_HI = 0;

    uint32_t target = timer_ticks + 1;
    while (timer_ticks < target) {
        // Spin waiting for timer interrupt to increment timer_ticks
    }
}

// ==========================================
// 8. Game logic
// ==========================================
void init_game() {
    snake_len = 3;
    snake_x[0] = WIDTH / 2; snake_y[0] = HEIGHT / 2;
    snake_x[1] = WIDTH / 2 - 1; snake_y[1] = HEIGHT / 2;
    snake_x[2] = WIDTH / 2 - 2; snake_y[2] = HEIGHT / 2;

    dir_x = 1; dir_y = 0;
    score = 0;
    game_over = false;

    apple_x = simple_rand() % WIDTH;
    apple_y = simple_rand() % HEIGHT;
}

void draw() {
    clear_screen();
    print_str("== SIMPLE-RV SNAKE ==\n");

    for (int i = 0; i < WIDTH + 2; i++) print_str("#");
    print_str("\n");

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (x == 0) print_str("#");

            if (x == apple_x && y == apple_y) {
                print_str("@");
            }
            else {
                bool is_snake = false;
                for (int k = 0; k < snake_len; k++) {
                    if (snake_x[k] == x && snake_y[k] == y) {
                        if (k == 0) print_str("O");
                        else print_str("o");
                        is_snake = true;
                        break;
                    }
                }
                if (!is_snake) print_str(" ");
            }

            if (x == WIDTH - 1) print_str("#");
        }
        print_str("\n");
    }

    for (int i = 0; i < WIDTH + 2; i++) print_str("#");
    print_str("\n");

    print_str("Score: ");
    char score_str[10];
    score_str[0] = (score / 10) + '0';
    score_str[1] = (score % 10) + '0';
    score_str[2] = '\0';
    print_str(score_str);
    print_str("\nUse W/A/S/D to move, Q to quit.\n");
}

void logic() {
    // Capture all buffered keystrokes during the frame
    while (uart_kbhit()) {
        push_key(uart_getchar());
    }

    char key = pop_key();
    if (key != '\0') {
        rand_state += 1;
        if (key == 'w' && dir_y == 0)      { dir_x = 0; dir_y = -1; }
        else if (key == 's' && dir_y == 0) { dir_x = 0; dir_y = 1; }
        else if (key == 'a' && dir_x == 0) { dir_x = -1; dir_y = 0; }
        else if (key == 'd' && dir_x == 0) { dir_x = 1; dir_y = 0; }
        else if (key == 'q') game_over = true;
    }

    // Move snake body
    for (int i = snake_len - 1; i > 0; i--) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    snake_x[0] += dir_x;
    snake_y[0] += dir_y;

    // Wall collision
    if (snake_x[0] < 0 || snake_x[0] >= WIDTH ||
        snake_y[0] < 0 || snake_y[0] >= HEIGHT) {
        game_over = true;
    }

    // Self collision
    for (int i = 1; i < snake_len; i++) {
        if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) {
            game_over = true;
        }
    }

    // Apple eaten
    if (snake_x[0] == apple_x && snake_y[0] == apple_y) {
        score++;
        if (snake_len < MAX_LEN) snake_len++;
        apple_x = simple_rand() % WIDTH;
        apple_y = simple_rand() % HEIGHT;
    }
}

// ==========================================
// Software arithmetic (RV32I has no M extension)
// ==========================================
uint32_t __udivsi3(uint32_t n, uint32_t d) {
    if (d == 0) return 0;
    uint32_t q = 0, r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (1 << i); }
    }
    return q;
}

uint32_t __umodsi3(uint32_t n, uint32_t d) {
    if (d == 0) return 0;
    uint32_t r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) r -= d;
    }
    return r;
}

int32_t __divsi3(int32_t n, int32_t d) {
    int sign = 1;
    if (n < 0) { n = -n; sign = -sign; }
    if (d < 0) { d = -d; sign = -sign; }
    return sign * __udivsi3(n, d);
}

int32_t __modsi3(int32_t n, int32_t d) {
    int sign = (n < 0) ? -1 : 1;
    if (n < 0) n = -n;
    if (d < 0) d = -d;
    return sign * __umodsi3(n, d);
}

int32_t __mulsi3(int32_t a, int32_t b) {
    uint32_t u_a = a, u_b = b, res = 0;
    while (u_b != 0) {
        if (u_b & 1) res += u_a;
        u_a <<= 1;
        u_b >>= 1;
    }
    return res;
}
