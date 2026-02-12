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

fptr sys_funcs[4] = {&do_a, &b, &c, &d};

void do_a(void)
{
    syscall(601);
}

__attribute__((constructor))
void a(void)
{
    volatile fptr fp = &do_a;
    //fptr fp = sys_funcs[0]; // Will add all a, b, and c to callgraph
    fp();
}


void b(void)
{
    syscall(602);
}

void d(void)
{
    syscall(604);
}

__attribute__((destructor))
void c(void)
{
    syscall(603);
}

int main(int argc, char **argv)
{
    volatile fptr fp = &b;

    fp();

    return 0;
}
