#ifndef __LIB_B_H__
#define __LIB_B_H__

typedef void (*fptr)(void);

struct bmodule {
    fptr fptr;
    int z;
};

struct amodule {
    int x;
    fptr ptr;
    struct bmodule bmod;
    struct bmodule *bmodp;
};


void bf3(void);
void bf1(void);

#endif
