
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("[PARENT] Starting program. PID: %ld\n", (long)getpid());
    fflush(stdout);

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        printf("[CHILD] Starting cat. PID: %ld\n", (long)getpid());
        fflush(stdout);

        execlp("cat", "cat", argv[1], (char *)NULL);

        /* Эта строка выполняется, только если execlp завершился ошибкой. */
        perror("execlp");
        _exit(127);
    }

    printf("[PARENT] Child PID: %ld\n", (long)pid);
    printf("[PARENT] I am the parent. Printing some text...\n");
    fflush(stdout);

    /* Ждём именно созданного дочернего процесса. */
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }

    if (WIFEXITED(status)) {
        printf("[PARENT] Child exited with status: %d\n",
               WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[PARENT] Child terminated by signal: %d\n",
               WTERMSIG(status));
    }

    printf("[PARENT] Child has finished. This is the last line.\n");

    return EXIT_SUCCESS;
}
