#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "liba.h"

int main(int argc, char **argv)
{
    int x = f(1);

    void *hdl = dlopen("liba.so", RTLD_NOW);
    int (*fptr)(int) = (int (*)(int)) dlsym(hdl, "g");

    int y = fptr(-1);

    return x + y;
}
