#include <stddef.h>
#include <stdlib.h>
#include "foo.h"

typedef void (*fptr)(void);

int
main(void)
{
    func0();
    func1();
    func2();
    bar_func0();

    exit(EXIT_SUCCESS);
}
