#ifndef PIPE_H
#define PIPE_H

#include "types.h"

#define PIPE_BUF_SIZE 4096

typedef struct {
    char buf[PIPE_BUF_SIZE];
    volatile int head;
    volatile int tail;
    volatile int readers;
    volatile int writers;
    volatile int closed;
} pipe_t;

int pipe_create(int *read_fd, int *write_fd);
int pipe_read(int pipe_id, char *buf, int len);
int pipe_write(int pipe_id, const char *buf, int len);
void pipe_close(int pipe_id, int is_writer);

#endif
