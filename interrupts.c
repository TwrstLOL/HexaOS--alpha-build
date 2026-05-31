#include "types.h"
#include "interrupts.h"

// IDT and handlers table
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

// Extern assembly stubs
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

extern void syscall_stub(void);

// Global tick counter
volatile uint32_t system_ticks = 0;

// Keyboard ring buffer
volatile char kbuf[KBUF_SIZE];
volatile int kbuf_head = 0, kbuf_tail = 0;

// Keyboard scancode tables
static const char kbd_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0,0
};
static const char kbd_map_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0,0
};
static int shift_pressed = 0;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void pic_remap(void) {
    outb(PIC1_CMD, 0x11); outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20); outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04); outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01); outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0x00); outb(PIC2_DATA, 0x00);
}

void pit_init(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);
}

void idt_init(void) {
    idtp.limit = sizeof(struct idt_entry) * IDT_ENTRIES - 1;
    idtp.base = (uint32_t)&idt;
    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].base_lo = 0;
        idt[i].base_hi = 0;
        idt[i].sel = 0;
        idt[i].always0 = 0;
        idt[i].flags = 0;
    }
    // Set ISR entries 0-31
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    // Set IRQ entries 32-47
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    // Set syscall gate (INT 0x80)
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE); // DPL=3
    // Load IDT
    __asm__ volatile("lidt %0" : : "m"(idtp));
}

// ---- GDT Setup ----
struct gdt_entry {
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t base_mi;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_hi;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt_entries[6];
static struct gdt_ptr gdtp;

extern void tss_flush(void);

void setup_gdt(void) {
    gdtp.limit = sizeof(struct gdt_entry) * 6 - 1;
    gdtp.base = (uint32_t)&gdt_entries;
    // Null descriptor
    gdt_entries[0].limit_lo = 0;
    gdt_entries[0].base_lo = 0;
    gdt_entries[0].base_mi = 0;
    gdt_entries[0].access = 0;
    gdt_entries[0].granularity = 0;
    gdt_entries[0].base_hi = 0;
    // Ring 0 Code: base=0, limit=4GB, 0x9A=present,ring0,code,readable
    gdt_entries[1].limit_lo = 0xFFFF;
    gdt_entries[1].base_lo = 0;
    gdt_entries[1].base_mi = 0;
    gdt_entries[1].access = 0x9A;
    gdt_entries[1].granularity = 0xCF;
    gdt_entries[1].base_hi = 0;
    // Ring 0 Data: base=0, limit=4GB, 0x92=present,ring0,data,rw
    gdt_entries[2].limit_lo = 0xFFFF;
    gdt_entries[2].base_lo = 0;
    gdt_entries[2].base_mi = 0;
    gdt_entries[2].access = 0x92;
    gdt_entries[2].granularity = 0xCF;
    gdt_entries[2].base_hi = 0;
    // Ring 3 Code: same but DPL=3, 0xFA
    gdt_entries[3].limit_lo = 0xFFFF;
    gdt_entries[3].base_lo = 0;
    gdt_entries[3].base_mi = 0;
    gdt_entries[3].access = 0xFA;
    gdt_entries[3].granularity = 0xCF;
    gdt_entries[3].base_hi = 0;
    // Ring 3 Data: same but DPL=3, 0xF2
    gdt_entries[4].limit_lo = 0xFFFF;
    gdt_entries[4].base_lo = 0;
    gdt_entries[4].base_mi = 0;
    gdt_entries[4].access = 0xF2;
    gdt_entries[4].granularity = 0xCF;
    gdt_entries[4].base_hi = 0;
    // TSS entry 5 (will be filled later with TSS address)
    gdt_entries[5].limit_lo = 0;
    gdt_entries[5].base_lo = 0;
    gdt_entries[5].base_mi = 0;
    gdt_entries[5].access = 0;
    gdt_entries[5].granularity = 0;
    gdt_entries[5].base_hi = 0;
    __asm__ volatile("lgdt %0" : : "m"(gdtp));
    // Reload segment registers
    __asm__ volatile(
        "jmp $0x08, $.reload_cs\n"
        ".reload_cs:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "eax"
    );
}

// Setup TSS entry in GDT
void setup_tss(uint32_t tss_addr) {
    uint32_t base = tss_addr;
    uint32_t limit = sizeof(uint32_t) * 26; // 104 bytes
    gdt_entries[5].limit_lo = limit & 0xFFFF;
    gdt_entries[5].base_lo = base & 0xFFFF;
    gdt_entries[5].base_mi = (base >> 16) & 0xFF;
    gdt_entries[5].access = 0x89; // present, ring 0, TSS (available)
    gdt_entries[5].granularity = (limit >> 16) & 0x0F;
    gdt_entries[5].base_hi = (base >> 24) & 0xFF;
}

// ---- Exception Handlers ----
static const char *exception_msgs[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt",
    "Breakpoint", "Overflow", "BOUND Range Exceeded",
    "Invalid Opcode", "Device Not Available", "Double Fault",
    "Coprocessor Segment Overrun", "Invalid TSS",
    "Segment Not Present", "Stack-Segment Fault",
    "General Protection Fault", "Page Fault",
    "Reserved", "x87 FPU Error", "Alignment Check",
    "Machine Check", "SIMD FPU Exception", "Virtualization",
    "Reserved","Reserved","Reserved","Reserved","Reserved",
    "Reserved","Reserved","Reserved","Reserved","Security"
};

// External terminal print function (defined in hexa.c)
extern void print_string(const char *str);
extern void print_color(const char *str, uint8_t color);
extern void put_char(char c, uint8_t color);

static void put_hex32(uint32_t val) {
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        int nib = val & 0xF;
        buf[i] = nib < 10 ? '0' + nib : 'A' + nib - 10;
        val >>= 4;
    }
    for (int i = 0; buf[i]; i++) put_char(buf[i], 0x0F);
}

void isr_handler(struct regs *r) {
    if (r->int_no == 14) {
        // Page Fault - read CR2 and return
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        print_color("[PF] ", 0x0C);
        print_color("at ", 0x0C);
        put_hex32(cr2);
        uint32_t err = r->err_code;
        print_color(" (", 0x0C);
        if (err & 1) print_color("present", 0x0C);
        else         print_color("not-present", 0x0C);
        if (err & 2) print_color("+write", 0x0C);
        else         print_color("+read", 0x0C);
        if (err & 4) print_color("+user", 0x0C);
        if (err & 8) print_color("+reserved", 0x0C);
        print_color(") ", 0x0C);
        put_hex32(r->eip);
        print_color("\n", 0x0C);
        // Don't panic — just report and return
        return;
    }
    if (r->int_no == 8) {
        print_color("[FATAL] Double Fault! Halting.\n", 0x4C);
        cli();
        while (1) hlt();
    }
    // General protection fault
    if (r->int_no == 13) {
        print_color("[GPF] ", 0x0C);
        put_hex32(r->eip);
        print_color(" err=", 0x0C);
        put_hex32(r->err_code);
        print_color(" (recovered)\n", 0x0C);
        return;
    }
    // Other exceptions: report and return
    if (r->int_no < 32) {
        print_color("[EXC] ", 0x0C);
        print_color(exception_msgs[r->int_no], 0x0C);
        print_color(" at ", 0x0C);
        put_hex32(r->eip);
        print_color(" (recovered)\n", 0x0C);
    }
}

void irq_handler(struct regs *r) {
    uint32_t irq = r->int_no - 32;
    // Timer tick
    if (irq == 0) {
        system_ticks++;
    }
    // Keyboard
    if (irq == 1) {
        uint8_t sc = inb(0x60);
        // Handle shift
        if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; goto pic_eoi; }
        if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; goto pic_eoi; }
        // Key release — ignore
        if (sc & 0x80) goto pic_eoi;
        // Convert scancode to char
        char c = shift_pressed ? kbd_map_shift[sc] : kbd_map[sc];
        if (c == 0) goto pic_eoi;
        // Push into ring buffer
        int next = (kbuf_head + 1) % KBUF_SIZE;
        if (next != kbuf_tail) {
            kbuf[kbuf_head] = c;
            kbuf_head = next;
        }
    }
pic_eoi:
    // Send EOI
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
    // Call scheduler on timer tick
    if (irq == 0) {
        extern void schedule(struct regs *r);
        schedule(r);
    }
}

int kb_getchar(void) {
    cli();
    if (kbuf_head == kbuf_tail) {
        sti();
        return -1;
    }
    char c = kbuf[kbuf_tail];
    kbuf_tail = (kbuf_tail + 1) % KBUF_SIZE;
    sti();
    return c;
}
