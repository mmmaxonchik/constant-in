#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/syscall.h>

#include "liba.h"

int main(int argc, char **argv)
{
    fptr fp;
    fp = f1();
    fp();

    return 0;
}
