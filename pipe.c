#include "types.h"
#include "pipe.h"
#include "process.h"

static pipe_t pipes[MAX_PIPES];
static int pipe_allocated[MAX_PIPES];

int pipe_create(int *read_fd, int *write_fd) {
    cli();
    int id = -1;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipe_allocated[i]) { id = i; break; }
    }
    if (id < 0) { sti(); return -1; }
    pipe_allocated[id] = 1;
    pipe_t *p = &pipes[id];
    p->head = 0;
    p->tail = 0;
    p->readers = 1;
    p->writers = 1;
    p->closed = 0;
    *read_fd = proc_alloc_fd(current_task, 2, id);
    *write_fd = proc_alloc_fd(current_task, 2, id + MAX_PIPES);
    sti();
    return 0;
}

int pipe_read(int pipe_id, char *buf, int len) {
    int id = pipe_id % MAX_PIPES;
    pipe_t *p = &pipes[id];
    int total = 0;
    while (total < len) {
        cli();
        if (p->head != p->tail) {
            buf[total++] = p->buf[p->tail];
            p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
            sti();
        } else {
            sti();
            if (p->writers == 0) return total;
            __asm__ volatile("pause");
        }
    }
    return total;
}

int pipe_write(int pipe_id, const char *buf, int len) {
    int id = pipe_id % MAX_PIPES;
    pipe_t *p = &pipes[id];
    int total = 0;
    while (total < len) {
        cli();
        int next = (p->head + 1) % PIPE_BUF_SIZE;
        if (next != p->tail) {
            p->buf[p->head] = buf[total++];
            p->head = next;
            sti();
        } else {
            sti();
            if (p->readers == 0) return total;
            __asm__ volatile("pause");
        }
    }
    return total;
}

void pipe_close(int pipe_id, int is_writer) {
    int id = pipe_id % MAX_PIPES;
    pipe_t *p = &pipes[id];
    cli();
    if (is_writer) p->writers--;
    else p->readers--;
    if (p->readers == 0 && p->writers == 0) {
        pipe_allocated[id] = 0;
    }
    sti();
}
