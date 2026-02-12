#include <unistd.h>
#include <syscall.h>

typedef void (*fptr)(void);

struct x {
    int a;
    int b;
};


void foo(void)
{
    syscall(600);
}


struct x thing = {
    .a = 1,
    .b = 2,
};


void bar(void)
{
    thing.a++;
}
