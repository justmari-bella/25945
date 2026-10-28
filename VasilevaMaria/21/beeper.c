
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t int_count = 0;
static volatile sig_atomic_t quit_requested = 0;

static void on_sigint(int sig)
{
    char bell = '\a';
    (void)sig;

    if (int_count < 2147483647)
        int_count++;

    /* write() допустим в обработчике сигнала. */
    (void)write(STDOUT_FILENO, &bell, 1);
}

static void on_sigquit(int sig)
{
    (void)sig;
    quit_requested = 1;
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    sa_int.sa_handler = on_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    sa_quit.sa_handler = on_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, NULL) == -1 ||
        sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    printf("Press Ctrl+C to beep; press Ctrl+\\ to exit.\n");
    fflush(stdout);

    while (!quit_requested)
        pause();

    printf("\nSIGINT received %d time(s). Exiting.\n",
           (int)int_count);

    return EXIT_SUCCESS;
}
