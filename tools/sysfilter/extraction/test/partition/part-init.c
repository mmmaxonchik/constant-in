// argtrack.c

#define USE_GLIBC

#ifdef USE_GLIBC
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>
#else
#include "test.h"
#endif

typedef void (*fptr)(void);

void do_a(void);
void a(void);
void b(void);
void c(void);
void d(void);

volatile fptr fp = NULL;

void do_a(void)
{
    syscall(601);
}

__attribute__((constructor))
void a(void)
{
    fp = &do_a;
}


int main(int argc, char **argv)
{
    fp();

    return 0;
}
