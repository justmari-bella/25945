
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <aio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/os_task32.sock"
#define MAX_CLIENTS 32
#define BUFFER_SIZE 1024

struct client {
    int fd;
    int active;
    struct aiocb request;
    char buffer[BUFFER_SIZE];
};

static struct client clients[MAX_CLIENTS];

static int start_read(struct client *c)
{
    memset(&c->request, 0, sizeof(c->request));

    c->request.aio_fildes = c->fd;
    c->request.aio_buf = c->buffer;
    c->request.aio_nbytes = sizeof(c->buffer);
    c->request.aio_offset = 0;

    c->request.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    c->request.aio_sigevent.sigev_signo = SIGUSR1;
    c->request.aio_sigevent.sigev_value.sival_ptr = c;

    if (aio_read(&c->request) == -1) {
        perror("aio_read");
        return -1;
    }

    return 0;
}

static void close_client(struct client *c)
{
    if (c->active) {
        close(c->fd);
        c->active = 0;
        c->fd = -1;
    }
}

int main(void)
{
    int server_fd;
    struct sockaddr_un addr;
    struct sigaction sa;
    sigset_t signals;
    struct timespec timeout;

    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);

    /*
     * Блокируем SIGUSR1, чтобы получать уведомления через
     * sigtimedwait(), а не выполнять обработчик сигнала.
     */
    sigemptyset(&signals);
    sigaddset(&signals, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &signals, NULL) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

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

    /*
     * Неблокирующий режим позволяет проверять новые подключения,
     * пока асинхронные чтения клиентов выполняются.
     */
    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1 ||
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("AIO server listening at %s\n", SOCKET_PATH);
    fflush(stdout);

    for (;;) {
        /* Принимаем все ожидающие подключения. */
        for (;;) {
            int fd = accept(server_fd, NULL, NULL);

            if (fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EINTR)
                    continue;

                perror("accept");
                break;
            }

            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].active) {
                    slot = i;
                    break;
                }
            }

            if (slot == -1) {
                fprintf(stderr, "Maximum number of clients reached\n");
                close(fd);
                continue;
            }

            clients[slot].fd = fd;
            clients[slot].active = 1;

            if (start_read(&clients[slot]) == -1)
                close_client(&clients[slot]);
        }

        /*
         * Ждём уведомление об окончании чтения.
         * Тайм-аут нужен, чтобы периодически проверять accept().
         */
        timeout.tv_sec = 0;
        timeout.tv_nsec = 100000000L;

        siginfo_t info;
        int sig = sigtimedwait(&signals, &info, &timeout);

        if (sig == -1) {
            if (errno == EAGAIN || errno == EINTR)
                continue;

            perror("sigtimedwait");
            break;
        }

        struct client *c = info.si_value.sival_ptr;
        if (c == NULL || !c->active)
            continue;

        int error = aio_error(&c->request);
        if (error == EINPROGRESS)
            continue;

        if (error != 0) {
            fprintf(stderr, "Asynchronous read failed: %s\n",
                    strerror(error));
            close_client(c);
            continue;
        }

        ssize_t n = aio_return(&c->request);

        if (n <= 0) {
            if (n == -1)
                perror("aio_return/read");
            close_client(c);
            continue;
        }

        for (ssize_t i = 0; i < n; i++) {
            c->buffer[i] =
                (char)toupper((unsigned char)c->buffer[i]);
        }

        size_t sent = 0;
        while (sent < (size_t)n) {
            ssize_t w = write(STDOUT_FILENO,
                              c->buffer + sent,
                              (size_t)n - sent);

            if (w == -1) {
                if (errno == EINTR)
                    continue;
                perror("write");
                break;
            }

            sent += (size_t)w;
        }

        /* Снова начинаем асинхронное чтение этого клиента. */
        if (c->active && start_read(c) == -1)
            close_client(c);
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            /*
             * Перед закрытием ждём завершения активного запроса.
             */
            while (aio_error(&clients[i].request) == EINPROGRESS) {
                const struct aiocb *list[1] = { &clients[i].request };
                aio_suspend(list, 1, NULL);
            }
            (void)aio_return(&clients[i].request);
            close_client(&clients[i]);
        }
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return EXIT_FAILURE;
}
