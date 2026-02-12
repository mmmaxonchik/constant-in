// argtrack.c
#define _GNU_SOURCE
#include <dlfcn.h>

#include "libargtrack.h"

int main(int argc, char **argv)
{
    a(); // OK
    b(); // OK
    c("printf");  // FAIL (intra-procedural)
    d(); // FAIL (RIP-relative)
    dlsym(RTLD_DEFAULT, argv[1]); // FAIL (arg)

    return 0;
}
