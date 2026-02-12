#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

int f(int x)
{
    return x;
}

int g(int x)
{
    long ret = syscall(555, 0xaa, 0xbb);

    return (int)ret;
}
