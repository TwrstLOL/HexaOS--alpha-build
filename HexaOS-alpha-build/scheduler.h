#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "interrupts.h"

#define MAX_TASKS 16
#define STACK_SIZE 4096

// Task states
#define TASK_READY   0
#define TASK_RUNNING 1
#define TASK_WAITING 2
#define TASK_DEAD    3

struct task {
    int pid;
    int state;
    uint32_t esp;      // kernel stack pointer
    uint32_t ebp;       
    uint32_t page_dir; // CR3 (for per-task address space)
    uint32_t kernel_esp; // Ring 0 stack pointer (for TSS)
    uint8_t stack[STACK_SIZE];
};

void scheduler_init(void);
int task_create(void (*entry)(void));
void task_exit(void);
void schedule(struct regs *r);
void switch_to_user(void);
void yield(void);

extern struct task tasks[MAX_TASKS];
extern int current_task;
extern int num_tasks;

#endif
