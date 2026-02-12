#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

/* #include "liba.h" */
/* #include "libb.h" */
/* #include "libd.h" */

int main(int argc, char **argv)
{
#if 0
    int x = 2;

    void *hdl = dlopen("libb.so", RTLD_NOW);
    int (*fptr)(int) = (int (*)(int)) dlsym(hdl, "h");

    int y = fptr(-2);
    return x + y;
#endif
    return 0;
}
