// argtrack.c
#define _GNU_SOURCE
#include <dlfcn.h>

#include "libat2.h"

void __attribute__((noinline)) f(char *sym)
{
    volatile dlsym_ptr dlp = &dlsym;
    volatile fptr p_ptr = &print_string;

    p_ptr(sym);
    dlp(RTLD_DEFAULT, sym);
}

int main(int argc, char **argv)
{
    f("printf");
    return 0;
}
