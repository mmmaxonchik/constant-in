#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "liba.h"

fptr fp_arr[] = {&f6, &f7};

void f8(void)
{
    syscall(508);
}

void f7(void)
{
    syscall(507);
    f8();
}

void f6(void)
{
    syscall(506);
}

void f5(void)
{
    syscall(505);
}

void f4(void)
{
    syscall(504);
    f5();
}
void f3(void)
{
    syscall(503);
}

fptr f2(void)
{
    syscall(502);
    return &f4;
}

fptr f1(void)
{
    syscall(501);
    return &f3;
}
