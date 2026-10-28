#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
int main(int argc, char *argv[])
{
    pid_t p;
    int status;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }
    printf("[PARENT] Starting: %s\n", argv[1]);
    fflush(stdout);
    p = fork();
    if (p < 0) { perror("fork"); return 1; }
    if (p == 0) {
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(127);
    }
    while (waitpid(p, &status, 0) < 0) {
        if (errno != EINTR) { perror("waitpid"); return 1; }
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        printf("Command exit code: %d\n", code);
        return code;
    }
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        const char *description = strsignal(sig);
        if (description != NULL)
            printf("Command killed by signal %d (%s)\n", sig, description);
        else
            printf("Command killed by signal %d\n", sig);
        return 128 + sig;
    }
    return 1;
}
