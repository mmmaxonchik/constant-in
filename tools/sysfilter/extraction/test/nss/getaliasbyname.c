#include <stddef.h>
#include <aliases.h>

int main(int argc, char **argv)
{
    struct aliasent *a = getaliasbyname(NULL);

    return a->alias_local;
}
