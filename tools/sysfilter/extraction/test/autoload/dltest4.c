#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "liba.h"
#include "libb.h"

int main(int argc, char **argv)
{
    int x = 2;

    void *hdl = dlopen("libb.so", RTLD_NOW);
    int (*fptr)(int) = (int (*)(int)) dlsym(hdl, "h");

    int y = fptr(-2);

    return x + y;
}
