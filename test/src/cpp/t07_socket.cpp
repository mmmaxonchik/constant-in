#include <unistd.h>
#include <sys/socket.h>

class Fd {
    int fd_;
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) (void)::close(fd_); }
    int get() const { return fd_; }
    void set(int fd) { fd_ = fd; }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

class SocketPairTest {
public:
    static int run() {
        int fds_raw[2];
        char b[1];

        (void)::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_raw);
        Fd a(fds_raw[0]);
        Fd bfd(fds_raw[1]);

        b[0] = 'A';
        (void)::sendto(a.get(), b, 1, 0, nullptr, 0);
        (void)::recvfrom(bfd.get(), b, 1, 0, nullptr, nullptr);
        (void)::shutdown(a.get(), SHUT_RDWR);

        return 0;
    }
};

int main() {
    return SocketPairTest::run();
}
