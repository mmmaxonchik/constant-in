#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>

int main(int argc, char **argv)
{
    struct hostent *he = gethostbyname("google.com");

    //printf("Result address length:  %d\n", he->h_length);
    return he->h_length;
}
