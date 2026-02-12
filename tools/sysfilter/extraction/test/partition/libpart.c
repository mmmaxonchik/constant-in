// libargtrack.c

#define _GNU_SOURCE
#include <dlfcn.h>

#include "libargtrack.h"

void a(void)
{
    dlsym(RTLD_DEFAULT, "printf");
}

void b(void)
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
