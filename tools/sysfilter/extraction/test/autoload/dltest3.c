#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "libb.h"

int main(int argc, char **argv)
{
    int x = 1;

    void *hdl = dlopen("libb.so", RTLD_NOW);
    int (*fptr)(int) = (int (*)(int)) dlsym(hdl, "h");

    int y = fptr(-1);

    return x + y;
}
