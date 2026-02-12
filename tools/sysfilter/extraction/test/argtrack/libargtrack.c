// libargtrack.c

#define _GNU_SOURCE
#include <dlfcn.h>

#include "libargtrack.h"

__attribute__((noinline))
void a(void) // OK
{
    dlsym(RTLD_DEFAULT, "printf");
}

__attribute__((noinline))
void b(void) // OK
{
    char *str = "printf";
    dlsym(RTLD_DEFAULT, str);
}


void c(char *sym)
{
    dlsym(RTLD_DEFAULT, sym);
}

const char *sym_d = "printf";
void d(void)
{
    dlsym(RTLD_DEFAULT, sym_d);
}
