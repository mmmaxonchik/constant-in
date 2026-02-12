#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main(int argc, char **argv)
{

    // Load cos from libm.so.6
#if 0
    void *h1 = dlopen("libm.so.6", RTLD_NOW);

    double (*cosine)(double) = (double (*)(double)) dlsym(h1, "cos");

    char str[20];
    snprintf(str, 20, "%g", (*cosine)(2.0));
    printf("%s\n", str);
#else
   char *str = "2.5";
#endif
    // Load atof from libc.so.6
    void *h2 = dlopen("libc.so.6", RTLD_NOW);
    double (*atof_ptr)(const char *) = (double (*)(const char *)) dlsym(h2, "atof");

    double v = (*atof_ptr)(str);
    printf("%g\n", v);

    return 0;
}
