#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>

class Fd {
    int fd_;
public:
    explicit Fd(int fd) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) (void)::syscall(SYS_close, fd_); }
    int get() const { return fd_; }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

class MemMapTest {
public:
    static int run() {
        int fd = (int)::syscall(SYS_memfd_create, "x", 0);
        Fd owned(fd);
        (void)::syscall(SYS_ftruncate, owned.get(), 4096);

        void* p = (void*)::syscall(SYS_mmap, 0, 4096, 3, 2, owned.get(), 0);
        (void)::syscall(SYS_mprotect, p, 4096, 1);
        (void)::syscall(SYS_munmap, p, 4096);

        return 0;
    }
};

int main() {
    return MemMapTest::run();
}
