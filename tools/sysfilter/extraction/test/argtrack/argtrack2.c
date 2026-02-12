// argtrack.c
#define _GNU_SOURCE
#include <dlfcn.h>

#include "libat2.h"

int main(int argc, char **argv)
{
    volatile fptr fp_print = &print_string;
    volatile fptr fp_c = &c;
    fp_print("foo");
    fp_c("printf");

    return 0;
}
