#ifndef PIPEFS_H
#define PIPEFS_H

#include <stdint.h>

#define PIPE_BUF_SIZE 4096

struct pipe_inode {
    char buf[PIPE_BUF_SIZE];
    volatile int read_pos;
    volatile int write_pos;
    volatile int readers;
    volatile int writers;
    volatile int refcount;
};

int vfs_pipe_create(int *fd0, int *fd1);

#endif