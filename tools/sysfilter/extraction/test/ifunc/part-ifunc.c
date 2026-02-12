// part-ifunc.c

#include "libifunc.h"

int main(int argc, char **argv)
{
   fptr ptr = &print;
   ptr(NULL, 0);
   //print(NULL, 0);

    return 0;
}
