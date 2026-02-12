
#ifndef __LIBAT2_H__
#define __LIBAT2_H__

typedef void (*fptr)(char *);
typedef void *(*dlsym_ptr)(void*, const char *);

void c(char *sym);
void print_string(char *name);
void f(char *sym);

#endif
