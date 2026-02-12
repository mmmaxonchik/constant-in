#include <unistd.h>
#include <syscall.h>

void foo(void)
{
    syscall(700);
}
