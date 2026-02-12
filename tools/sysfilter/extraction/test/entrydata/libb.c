#include "test.h"

#ifdef __USE_GLIBC__
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include "liba.h"
#include "libb.h"

void bf1(void)
{
    syscall(601);
}

void bf2(void)
{
    syscall(602);
}

void bf3(void)
{
    syscall(603);
}

struct bmodule bmod = {
    &f3,
    3,
};

struct amodule xdata = {
    1,
    &bf2,
    {&bf1, 2},
    &bmod,
};
