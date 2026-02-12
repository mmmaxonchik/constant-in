// libd.c

unsigned int arr[4];

int d_f(int x)
{
    return arr[x & 0x3];
}
