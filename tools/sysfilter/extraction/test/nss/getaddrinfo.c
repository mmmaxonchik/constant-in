#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo  *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = getaddrinfo("google.com", "80", &hints, &result);
    if (rv != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	exit(EXIT_FAILURE);
    }

    return 0;
}
