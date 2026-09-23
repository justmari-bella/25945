#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

extern char **environ;

/* Структура для хранения опций в порядке появления */
#define MAX_OPTS 256

typedef struct {
    char opt;        /* символ опции */
    char *arg;       /* аргумент (если есть) */
} Option;

static Option opts[MAX_OPTS];
static int opt_count = 0;

/* Добавить опцию в список */
static void add_option(char opt, char *arg) {
    if (opt_count < MAX_OPTS) {
        opts[opt_count].opt = opt;
        opts[opt_count].arg = arg ? strdup(arg) : NULL;
        opt_count++;
    }
}

/* -i: печать реальных и эффективных UID/GID */
static void do_i(void) {
    printf("Real UID      = %d\n", (int)getuid());
    printf("Effective UID = %d\n", (int)geteuid());
    printf("Real GID      = %d\n", (int)getgid());
    printf("Effective GID = %d\n", (int)getegid());
}

/* -s: стать лидером группы */
static void do_s(void) {
    if (setpgid(0, 0) == 0) {
        printf("Process became group leader. PGID = %d\n", (int)getpgrp());
    } else {
        perror("setpgid");
    }
}

/* -p: PID, PPID, PGID */
static void do_p(void) {
    printf("PID  = %d\n", (int)getpid());
    printf("PPID = %d\n", (int)getppid());
    printf("PGID = %d\n", (int)getpgrp());
}

/* -u: вывод ulimit (RLIMIT_NOFILE) */
static void do_u(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        printf("ulimit (RLIMIT_NOFILE): soft = %ld, hard = %ld\n",
               (long)rl.rlim_cur, (long)rl.rlim_max);
    } else {
        perror("getrlimit");
    }
}

/* -U: изменение ulimit */
static void do_U(char *arg) {
    if (!arg) {
        fprintf(stderr, "-U requires an argument\n");
        return;
    }
    errno = 0;
    char *endptr;
    long val = strtol(arg, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || val < 0) {
        fprintf(stderr, "Invalid value for -U: %s\n", arg);
        return;
    }
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)val;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
        perror("setrlimit");
    } else {
        printf("ulimit changed to %ld\n", val);
    }
}

/* -c: печать размера core-файла */
static void do_c(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        printf("Core file size: soft = %ld bytes, hard = %ld bytes\n",
               (long)rl.rlim_cur, (long)rl.rlim_max);
    } else {
        perror("getrlimit");
    }
}

/* -C: изменение размера core-файла */
static void do_C(char *arg) {
    if (!arg) {
        fprintf(stderr, "-C requires an argument\n");
        return;
    }
    errno = 0;
    char *endptr;
    long val = strtol(arg, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || val < 0) {
        fprintf(stderr, "Invalid value for -C: %s\n", arg);
        return;
    }
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) != 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)val;
    if (setrlimit(RLIMIT_CORE, &rl) != 0) {
        perror("setrlimit");
    } else {
        printf("Core file size changed to %ld bytes\n", val);
    }
}

/* -d: текущая рабочая директория */
static void do_d(void) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != NULL) {
        printf("Current directory: %s\n", buf);
    } else {
        perror("getcwd");
    }
}

/* -v: печать переменных среды */
static void do_v(void) {
    printf("Environment variables:\n");
    for (char **ep = environ; *ep != NULL; ep++) {
        printf("  %s\n", *ep);
    }
}

/* -V: установка переменной среды */
static void do_V(char *arg) {
    if (!arg) {
        fprintf(stderr, "-V requires an argument NAME=value\n");
        return;
    }
    if (putenv(arg) != 0) {
        perror("putenv");
    } else {
        printf("Environment variable set: %s\n", arg);
    }
}

int main(int argc, char *argv[]) {
    char *options = "ispuU:cC:dvV:";
    int c;

    /* Сбор всех опций (getopt обрабатывает слева направо) */
    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'i': case 's': case 'p': case 'u':
            case 'c': case 'd': case 'v':
                add_option((char)c, NULL);
                break;
            case 'U': case 'C': case 'V':
                add_option((char)c, optarg);
                break;
            case '?':
                fprintf(stderr, "Invalid option: -%c\n", optopt);
                break;
            default:
                break;
        }
    }

    /* Выполнение опций в порядке справа налево */
    for (int i = opt_count - 1; i >= 0; i--) {
        switch (opts[i].opt) {
            case 'i': do_i(); break;
            case 's': do_s(); break;
            case 'p': do_p(); break;
            case 'u': do_u(); break;
            case 'U': do_U(opts[i].arg); break;
            case 'c': do_c(); break;
            case 'C': do_C(opts[i].arg); break;
            case 'd': do_d(); break;
            case 'v': do_v(); break;
            case 'V': do_V(opts[i].arg); break;
        }
    }

    /* Освобождение памяти */
    for (int i = 0; i < opt_count; i++) {
        free(opts[i].arg);
    }

    return 0;
}