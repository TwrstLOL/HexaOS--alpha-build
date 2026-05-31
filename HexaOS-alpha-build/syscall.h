#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "interrupts.h"

#define SYS_PRINT  0
#define SYS_READ   1
#define SYS_EXIT   2
#define SYS_TICKS  3

uint32_t syscall_handler(struct regs *r);

#endif
