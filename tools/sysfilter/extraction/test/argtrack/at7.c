// at7.c

__attribute__((noinline))
long __sysfilter_argtrack_test(long x)
{
    return x;
}

int main(int argc, char **argv)
{
    long arg = (argc > 0) ? 42 : 200;
    long x = __sysfilter_argtrack_test(arg);

    return x;
}
