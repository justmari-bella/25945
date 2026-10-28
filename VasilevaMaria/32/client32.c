
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/os_task32.sock"

int main(void)
{
    int fd;
    struct sockaddr_un addr;
    char buffer[1024];

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_PATH);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Enter text. Press Ctrl+D to disconnect:\n");

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t length = strlen(buffer);
        size_t sent = 0;

        while (sent < length) {
            ssize_t n = write(fd, buffer + sent, length - sent);

            if (n == -1) {
                perror("write");
                close(fd);
                return EXIT_FAILURE;
            }

            sent += (size_t)n;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
