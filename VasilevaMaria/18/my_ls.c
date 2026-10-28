
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

static void make_permissions(mode_t mode, char p[11])
{
    if (S_ISDIR(mode))
        p[0] = 'd';
    else if (S_ISREG(mode))
        p[0] = '-';
    else if (S_ISLNK(mode))
        p[0] = 'l';
    else
        p[0] = '?';

    p[1] = (mode & S_IRUSR) ? 'r' : '-';
    p[2] = (mode & S_IWUSR) ? 'w' : '-';
    p[3] = (mode & S_IXUSR) ? 'x' : '-';

    p[4] = (mode & S_IRGRP) ? 'r' : '-';
    p[5] = (mode & S_IWGRP) ? 'w' : '-';
    p[6] = (mode & S_IXGRP) ? 'x' : '-';

    p[7] = (mode & S_IROTH) ? 'r' : '-';
    p[8] = (mode & S_IWOTH) ? 'w' : '-';
    p[9] = (mode & S_IXOTH) ? 'x' : '-';
    p[10] = '\0';
}

int main(int argc, char *argv[])
{
    int errors = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> [file ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        char permissions[11];
        char date[32];
        char path_copy[PATH_MAX];
        char *name;
        const char *owner_name = "?";
        const char *group_name = "?";
        struct passwd *pw;
        struct group *gr;
        struct tm *tm_info;
        time_t now;

        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            errors = 1;
            continue;
        }

        make_permissions(st.st_mode, permissions);

        pw = getpwuid(st.st_uid);
        if (pw != NULL)
            owner_name = pw->pw_name;

        gr = getgrgid(st.st_gid);
        if (gr != NULL)
            group_name = gr->gr_name;

        tm_info = localtime(&st.st_mtime);
        if (tm_info != NULL) {
            now = time(NULL);

            if (now != (time_t)-1 &&
                difftime(now, st.st_mtime) > 183.0 * 24 * 60 * 60) {
                strftime(date, sizeof(date), "%b %e  %Y", tm_info);
            } else {
                strftime(date, sizeof(date), "%b %e %H:%M", tm_info);
            }
        } else {
            strcpy(date, "?");
        }

        if (strlen(argv[i]) >= sizeof(path_copy)) {
            fprintf(stderr, "%s: path is too long\n", argv[i]);
            errors = 1;
            continue;
        }

        strcpy(path_copy, argv[i]);
        name = basename(path_copy);

        printf("%-10s %3lu %-8s %-8s ",
               permissions,
               (unsigned long)st.st_nlink,
               owner_name,
               group_name);

        if (S_ISREG(st.st_mode))
            printf("%10lld ", (long long)st.st_size);
        else
            printf("%10s ", "");

        printf("%-12s %s\n", date, name);
    }

    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}
