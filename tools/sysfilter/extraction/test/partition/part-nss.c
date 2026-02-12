// argtrack.c

#define USE_GLIBC

#ifdef USE_GLIBC
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>
#include <netdb.h>
#else
#include "test.h"
#endif

typedef void (*fptr)(void);


int main(int argc, char **argv)
{
    gethostbyname("aaaaaaa");
    syscall(601);

    return 0;
}
