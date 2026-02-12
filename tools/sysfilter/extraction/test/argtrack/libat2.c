// libargtrack.c
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "libat2.h"

#if 0
struct dl_funcs {
    fptr dlsym;
    fptr not_dlsym;
}

struct dl_funcs dl1 = {
    c,
    print_string,
};
#endif

void c(char *sym)
{
    dlsym(RTLD_DEFAULT, sym);
}

void print_string(char *name)
{
    printf("%s\n", name);
}
