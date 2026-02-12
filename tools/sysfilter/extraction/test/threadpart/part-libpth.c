// argtrack.c
#include <pthread.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void libpth_do_thread(void);

int main(int argc, char **argv)
{
    libpth_do_thread();

    return 0;

}
