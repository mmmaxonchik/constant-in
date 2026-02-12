#include <dlfcn.h>

#include "libb.h"

int h(int x)
{
    void *hdl = dlopen("liba.so", RTLD_NOW);
    int (*fptr)(int) = (int (*)(int)) dlsym(hdl, "g");

    return fptr(x);
}

int b_f1(int x)
{
    return x;
}
