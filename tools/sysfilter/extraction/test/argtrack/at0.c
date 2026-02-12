// argtrack.c
#define _GNU_SOURCE
#include <dlfcn.h>

int main(int argc, char **argv)
{
    dlsym(RTLD_DEFAULT, "printf");

    return 0;
}
