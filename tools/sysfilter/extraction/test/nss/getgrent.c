#include <grp.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
    struct group *ret = getgrent();

    return ret->gr_gid;
}
