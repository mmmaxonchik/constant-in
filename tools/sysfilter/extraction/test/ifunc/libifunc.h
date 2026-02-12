#ifndef __TEST_LIB_IFUNC_H__
#define __TEST_LIB_IFUNC_H__

#include <stddef.h>
#include <syscall.h>
#include <unistd.h>

typedef void (*fptr)(char *, size_t);

void copy(char *buf, size_t len);
void print(char *buf, size_t len);
void foo(void);

#endif
