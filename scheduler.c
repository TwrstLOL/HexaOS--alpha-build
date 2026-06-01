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
    tasks[current_task].esp = (uint32_t)r;
    int next = current_task;
    for (int i = 1; i < MAX_TASKS; i++) {
        int candidate = (current_task + i) % MAX_TASKS;
        if (tasks[candidate].state == TASK_READY) {
            next = candidate;
            break;
        }
    }
    if (next == current_task) return;
    tasks[current_task].state = TASK_READY;
    current_task = next;
    tasks[next].state = TASK_RUNNING;
    tss[0] = tasks[next].kernel_esp;
    // Directly restore the next task's saved register state.
    // The task stack layout (built by task_create) matches:
    //   [DS|ES|FS|GS] [EDI|ESI|EBP|ESP_skip|EBX|EDX|ECX|EAX] [int_no|err_code] [EIP|CS|EFLAGS|user_ESP|user_SS]
    // We skip the 4 segment registers (16 bytes), POPA the general regs,
    // skip int_no/err_code (8 bytes), then IRET back to the task.
    __asm__ volatile(
        "mov %0, %%esp\n"
        "add $16, %%esp\n"
        "popa\n"
        "add $8, %%esp\n"
        "iret"
        :
        : "r"(tasks[next].esp)
        : "memory"
    );
    /* NOT REACHED */
}

int task_create_user(void (*entry)(void)) {
    if (num_tasks >= MAX_TASKS) return -1;
    int pid = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) { pid = i; break; }
    }
    if (pid < 0) return -1;
    struct task *t = &tasks[pid];
    t->pid = pid;
    t->state = TASK_READY;
    t->page_dir = 0;
    // User stack (4KB allocated from kernel space)
    static uint8_t user_stacks[MAX_TASKS][4096];
    uint32_t user_stack_top = (uint32_t)user_stacks[pid] + 4096;
    uint32_t *sp = (uint32_t *)((uint32_t)user_stacks[pid] + 4096);
    // Build IRET frame for ring 3 transition
    *--sp = 0x20;                     // SS = ring 3 data segment
    *--sp = user_stack_top;           // user ESP (fresh stack top)
    *--sp = 0x202;                    // EFLAGS (IF set)
    *--sp = 0x18;                     // CS = ring 3 code segment
    *--sp = (uint32_t)entry;          // EIP
    *--sp = 0;                        // error code (dummy)
    *--sp = 0;                        // int number (dummy)
    // pusha registers
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    // segment registers
    *--sp = 0x20; *--sp = 0x20; *--sp = 0x20; *--sp = 0x20;
    t->esp = (uint32_t)sp;
    t->ebp = 0;
    t->kernel_esp = (uint32_t)t->stack + STACK_SIZE;
    num_tasks++;
    return pid;
}

void switch_to_user(void) {
    // This is called from the init task to enter user mode.
    extern void _user_entry(void);
    static uint8_t user_stack[4096];
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
