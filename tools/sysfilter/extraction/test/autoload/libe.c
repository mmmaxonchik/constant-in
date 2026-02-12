// libe.c
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

static int local_func(int x)
{
    return x;
}

int global_func(int x)
{
    return x;
}

int do_test(void *hdl, char *sym_name)
{
    dlerror();
    int (*fptr)(int) = (int (*)(int))dlsym(hdl, sym_name);
    char *msg = dlerror();

    if (msg != NULL) {
	printf("%s:  dlsym returned error:  %s\n", sym_name, msg);
	return 1;
    };

    return 0;
}

int e_f(int x)
{
    void *hdl = dlopen("/home/deemer/Development/sysfilter-spec/extraction/test/autoload/libe.so", RTLD_NOW);

    do_test(RTLD_DEFAULT, "local_func");
    do_test(RTLD_DEFAULT, "global_func");
    do_test(hdl, "local_func");
    do_test(hdl, "global_func");

    return 0;
}
