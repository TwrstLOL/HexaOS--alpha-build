#include "types.h"
#include "syscall.h"
#include "interrupts.h"

extern void print_string(const char *str);
extern void print_color(const char *str, uint8_t color);
extern int kb_getchar(void);

uint32_t syscall_handler(struct regs *r) {
    uint32_t num = r->eax;
    uint32_t arg1 = r->ebx;
    uint32_t arg2 = r->ecx;
    uint32_t arg3 = r->edx;
    (void)arg2; (void)arg3;
    switch (num) {
        case SYS_PRINT: {
            // arg1 = pointer to string in user space
            const char *str = (const char *)arg1;
            // For now, just print it (user and kernel share address space)
            print_string(str);
            return 0;
        }
        case SYS_READ: {
            // arg1 = buffer, arg2 = max_len
            // Read a single char from keyboard (blocking poll)
            char *buf = (char *)arg1;
            int max = (int)arg2;
            if (max < 1) return 0;
            int c;
            do {
                c = kb_getchar();
            } while (c < 0);
            buf[0] = (char)c;
            if (max > 1 && c != '\n') {
                buf[1] = '\n';
                return 2;
            }
            return 1;
        }
        case SYS_EXIT: {
            extern void task_exit(void);
            task_exit();
            return 0;
        }
        case SYS_TICKS: {
            return system_ticks;
        }
        default:
            return 0xFFFFFFFF;
    }
}
