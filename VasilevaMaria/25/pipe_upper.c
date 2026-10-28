
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

int main(void)
{
    int fd[2];
    pid_t pid;
    const char message[] =
        "Hello from the parent process! Mixed Case Text.\n";

    if (pipe(fd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        char buffer[256];
        ssize_t n;

        close(fd[1]);

        while ((n = read(fd[0], buffer, sizeof(buffer))) > 0) {
            for (ssize_t i = 0; i < n; i++)
                buffer[i] = (char)toupper((unsigned char)buffer[i]);

            if (write(STDOUT_FILENO, buffer, (size_t)n) == -1) {
                perror("write");
                close(fd[0]);
                _exit(EXIT_FAILURE);
            }
        }

        if (n == -1)
            perror("read");

        close(fd[0]);
        _exit(n == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
    }

    close(fd[0]);

    size_t length = strlen(message);
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = write(fd[1], message + sent, length - sent);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("write");
            close(fd[1]);
            return EXIT_FAILURE;
        }

        sent += (size_t)n;
    }

    close(fd[1]);

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
