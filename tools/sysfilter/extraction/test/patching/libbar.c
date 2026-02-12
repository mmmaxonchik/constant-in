#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "foo.h"
#include "sys.h"

void
bar_func0(void)
{
    func0();
}

void
bar_func1(void)
{
    func1();
}
