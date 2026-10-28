
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/os_task31.sock"

int main(void)
{
    int server_fd;
    struct sockaddr_un addr;
    fd_set active, readable;
    char buffer[1024];

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    unlink(SOCKET_PATH);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(server_fd, 16) == -1) {
        perror("bind/listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    FD_ZERO(&active);
    FD_SET(server_fd, &active);

    printf("Multi-client server listening at %s\n", SOCKET_PATH);
    fflush(stdout);

    for (;;) {
        readable = active;

        if (select(FD_SETSIZE, &readable, NULL, NULL, NULL) == -1) {
            perror("select");
            break;
        }

        for (int fd = 0; fd < FD_SETSIZE; fd++) {
            if (!FD_ISSET(fd, &readable))
                continue;

            if (fd == server_fd) {
                int client_fd = accept(server_fd, NULL, NULL);

                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                if (client_fd >= FD_SETSIZE) {
                    fprintf(stderr, "Too many file descriptors\n");
                    close(client_fd);
                    continue;
                }

                FD_SET(client_fd, &active);
            } else {
                ssize_t n = read(fd, buffer, sizeof(buffer));

                if (n <= 0) {
                    if (n == -1)
                        perror("read");

                    close(fd);
                    FD_CLR(fd, &active);
                    continue;
                }

                for (ssize_t i = 0; i < n; i++)
                    buffer[i] =
                        (char)toupper((unsigned char)buffer[i]);

                size_t sent = 0;
                while (sent < (size_t)n) {
                    ssize_t w = write(STDOUT_FILENO,
                                      buffer + sent,
                                      (size_t)n - sent);
                    if (w == -1) {
                        perror("write");
                        break;
                    }
                    sent += (size_t)w;
                }
            }
        }
    }

    for (int fd = 0; fd < FD_SETSIZE; fd++) {
        if (FD_ISSET(fd, &active))
            close(fd);
    }

    unlink(SOCKET_PATH);
    return EXIT_FAILURE;
}
