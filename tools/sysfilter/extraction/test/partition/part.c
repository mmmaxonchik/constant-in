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

__attribute__((constructor))
void a(void)
{
    syscall(601);
    //printf("601\n");
}

void b(void)
{
    syscall(602);
    //printf("602\n");
}

__attribute__((destructor))
void c(void)
{
    syscall(603);
    //printf("602\n");
}


int main(int argc, char **argv)
{
    b();

    return 0;
}
