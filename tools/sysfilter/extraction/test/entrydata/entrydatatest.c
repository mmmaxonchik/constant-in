#include "test.h"

#ifdef __USE_GLIBC__
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/syscall.h>
#endif

#include "libb.h"

extern struct bmodule bmod;

int main(int argc, char **argv)
{
    struct bmodule *b = &bmod;

    return b->z;
}
