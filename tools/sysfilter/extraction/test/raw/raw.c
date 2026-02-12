#include <unistd.h>
#include <sys/syscall.h>

void test_raw_ok(void)
{
    asm("mov $512, %rax");
    asm("syscall");
}

void test_raw_fail(void)
{
    asm("syscall");
}

void test_func_fail(int x)
{
    syscall(x);
}

void test_fail_arg(int x)
{
    syscall(x);
}

int main(int argc, char **argv)
{
    syscall(514, 0xaa, 0xbb);

    test_raw_ok();
    test_raw_fail();
    test_fail_arg(513);
    test_func_fail(argc);

    return 0;
}
