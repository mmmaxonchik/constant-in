// argtrack.c
#define _GNU_SOURCE
#include <dlfcn.h>

typedef void *(*dlsym_ptr)(void*, const char *);

int main(int argc, char **argv)
{
    volatile dlsym_ptr dlp = &dlsym;
    dlp(RTLD_DEFAULT, "printf");

    return 0;
}
