#define _GNU_SOURCE
#include <dlfcn.h>



int main(int argc, char **argv)
{
    char *str = "printf";
    dlsym(RTLD_DEFAULT, str);

    return 0;

}
