
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/os_task30.sock"

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_un addr;
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

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("Server waiting at %s\n", SOCKET_PATH);
    fflush(stdout);

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    for (;;) {
        ssize_t n = read(client_fd, buffer, sizeof(buffer));

        if (n == 0)
            break;

        if (n == -1) {
            perror("read");
            break;
        }

        for (ssize_t i = 0; i < n; i++)
            buffer[i] = (char)toupper((unsigned char)buffer[i]);

        size_t sent = 0;
        while (sent < (size_t)n) {
            ssize_t w = write(STDOUT_FILENO, buffer + sent,
                              (size_t)n - sent);
            if (w == -1) {
                perror("write");
                close(client_fd);
                close(server_fd);
                unlink(SOCKET_PATH);
                return EXIT_FAILURE;
            }
            sent += (size_t)w;
        }
    }

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    return EXIT_SUCCESS;
}
