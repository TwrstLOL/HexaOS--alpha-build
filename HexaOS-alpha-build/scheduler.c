#include "types.h"
#include "scheduler.h"
#include "interrupts.h"

struct task tasks[MAX_TASKS];
int current_task = 0;
int num_tasks = 0;

// TSS (Task State Segment)
static uint32_t tss[26] __attribute__((aligned(4)));

extern void tss_flush(void);
extern void setup_tss(uint32_t tss_addr);
extern void switch_task(uint32_t *old_esp, uint32_t new_esp);
extern void enter_userspace(uint32_t entry, uint32_t stack, uint32_t cs, uint32_t ds);

void scheduler_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].pid = -1;
        tasks[i].state = TASK_DEAD;
        tasks[i].esp = 0;
    }
    // Initialize TSS
    for (int i = 0; i < 26; i++) tss[i] = 0;
    // Set TSS SS0 to kernel data segment, ESP0 will be set per-task
    tss[1] = 0x10; // SS0 (ring 0 stack segment)
    setup_tss((uint32_t)tss);
    tss_flush();
}

int task_create(void (*entry)(void)) {
    if (num_tasks >= MAX_TASKS) return -1;
    int pid = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) { pid = i; break; }
    }
    if (pid < 0) return -1;
    struct task *t = &tasks[pid];
    t->pid = pid;
    t->state = TASK_READY;
    t->page_dir = 0; // use kernel's page dir
    // Set up kernel stack for this task
    uint32_t *stack_top = (uint32_t *)((uint32_t)t->stack + STACK_SIZE);
    // Initial stack frame for when this task first runs
    // We push a fake IRET frame and some registers
    *--stack_top = 0x10;          // SS (kernel data segment)
    *--stack_top = (uint32_t)t->stack + STACK_SIZE; // ESP
    *--stack_top = 0x202;         // EFLAGS (IF set)
    *--stack_top = 0x08;          // CS (kernel code segment)
    *--stack_top = (uint32_t)entry; // EIP
    // Push error code and int number (matching ISR frame)
    *--stack_top = 0;             // error code
    *--stack_top = 0;             // int number
    // Push general registers (matching pusha order)
    *--stack_top = 0; // EAX
    *--stack_top = 0; // ECX
    *--stack_top = 0; // EDX
    *--stack_top = 0; // EBX
    *--stack_top = 0; // ESP (will be set)
    *--stack_top = 0; // EBP
    *--stack_top = 0; // ESI
    *--stack_top = 0; // EDI
    // Push segment registers
    *--stack_top = 0x10; // GS
    *--stack_top = 0x10; // FS
    *--stack_top = 0x10; // ES
    *--stack_top = 0x10; // DS
    // This ESP value will be restored when switching to this task
    t->esp = (uint32_t)stack_top;
    t->ebp = 0;
    t->kernel_esp = (uint32_t)t->stack + STACK_SIZE;
    num_tasks++;
    return pid;
}

void task_exit(void) {
    tasks[current_task].state = TASK_DEAD;
    tasks[current_task].pid = -1;
    num_tasks--;
    // Find next task
    for (int i = 0; i < MAX_TASKS; i++) {
        int next = (current_task + 1 + i) % MAX_TASKS;
        if (tasks[next].state == TASK_READY || tasks[next].state == TASK_RUNNING) {
            // Switch to next task
            tasks[next].state = TASK_RUNNING;
            current_task = next;
            // Update TSS ESP0
            tss[0] = tasks[next].kernel_esp; // ESP0
            // Restore task's stack
            __asm__ volatile(
                "mov %0, %%esp\n"
                "popa\n"
                "add $8, %%esp\n"
                "iret"
                : : "r"(tasks[next].esp) : "memory"
            );
        }
    }
    // No tasks left — halt
    cli();
    while (1) hlt();
}

void schedule(struct regs *r) {
    if (num_tasks <= 1) return;
    // Save current task's register state pointer (r points to the regs struct on stack)
    tasks[current_task].esp = (uint32_t)r;
    // Find next ready task
    int next = current_task;
    for (int i = 1; i < MAX_TASKS; i++) {
        int candidate = (current_task + i) % MAX_TASKS;
        if (tasks[candidate].state == TASK_READY) {
            next = candidate;
            break;
        }
    }
    if (next == current_task) return;
    // Switch to next task
    tasks[current_task].state = TASK_READY;
    current_task = next;
    tasks[next].state = TASK_RUNNING;
    tss[0] = tasks[next].kernel_esp;
    // Copy next task's saved register state into the current stack's regs struct
    // When irq_common pops the regs and IRETs, it will return to the next task
    struct regs *next_regs = (struct regs *)tasks[next].esp;
    r->gs = next_regs->gs;
    r->fs = next_regs->fs;
    r->es = next_regs->es;
    r->ds = next_regs->ds;
    r->edi = next_regs->edi;
    r->esi = next_regs->esi;
    r->ebp = next_regs->ebp;
    r->esp = next_regs->esp;
    r->ebx = next_regs->ebx;
    r->edx = next_regs->edx;
    r->ecx = next_regs->ecx;
    r->eax = next_regs->eax;
    r->int_no = next_regs->int_no;
    r->err_code = next_regs->err_code;
    r->eip = next_regs->eip;
    r->cs = next_regs->cs;
    r->eflags = next_regs->eflags;
    r->user_esp = next_regs->user_esp;
    r->user_ss = next_regs->user_ss;
}

void switch_to_user(void) {
    // This is called from the init task to enter user mode.
    // Setup user code/data segments and jump to user space.
    extern void _user_entry(void);
    // Use a simple user stack
    static uint8_t user_stack[4096];
    // Enter user mode at _user_entry with ring 3 segments
    enter_userspace(
        (uint32_t)_user_entry,
        (uint32_t)user_stack + 4096,
        0x18,  // Ring 3 code segment (GDT index 3, RPL=3)
        0x20   // Ring 3 data segment (GDT index 4, RPL=3)
    );
}

void yield(void) {
    __asm__ volatile("int $0x20"); // trigger timer IRQ to reschedule
}
