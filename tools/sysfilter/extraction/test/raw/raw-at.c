#include <unistd.h>
#include <sys/syscall.h>

typedef long (*fptr)(long, ...);

int main(int argc, char **argv)
{
    volatile fptr ptr = &syscall;
    ptr(600);

    return 0;
}
