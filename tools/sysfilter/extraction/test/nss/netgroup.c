#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>

int main(int argc, char **argv)
{
    int rv = innetgr(NULL, NULL, NULL, NULL);

    //printf("Result address length:  %d\n", he->h_length);
    return rv;
}
