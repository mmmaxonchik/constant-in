#include <unistd.h>
#include <sys/socket.h>

int main(void) {
    int fds[2];
    char b[1];

    (void)socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    b[0] = 'A';
    (void)sendto(fds[0], b, 1, 0, 0, 0);
    (void)recvfrom(fds[1], b, 1, 0, 0, 0);

    (void)shutdown(fds[0], SHUT_RDWR);
    (void)close(fds[0]);
    (void)close(fds[1]);

    return 0;
}
