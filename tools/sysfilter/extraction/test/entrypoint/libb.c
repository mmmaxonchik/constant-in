#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "libb.h"

int bf2(void)
{
    syscall(602);
    struct hostent *he = gethostbyname("google.com");
    return he->h_length;
}

int bf1(void)
{
    syscall(601);
    return 601;
}
