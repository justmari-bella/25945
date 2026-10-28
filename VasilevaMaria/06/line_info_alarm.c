#include <stdio.h>      /* printf, scanf, fflush, perror  */
#include <stdlib.h>     /* malloc, realloc, free, EXIT_*  */
#include <unistd.h>     /* read, lseek, close, write, alarm, sysconf */
#include <fcntl.h>      /* open, O_RDONLY                 */
#include <signal.h>     /* signal, SIGALRM                */
#include <errno.h>      /* errno                          */
#include <sys/time.h>   /* gettimeofday                   */
#include <string.h>     /* memset                         */

#define BLOCK_SIZE      4096
#define TIMEOUT_SECONDS 5

/* Глобальные данные */
static int g_fd = -1;
static volatile sig_atomic_t g_timeout_triggered = 0;
static struct timeval g_start_time; /* время начала чтения */

/* ---------- таблица строк ---------- */
typedef struct {
    long offset;    /* позиция начала строки в файле */
    int  length;    /* длина строки без '\n'         */
} LineInfo;

typedef struct {
    LineInfo *items; // указатель на динамический массив записей
    size_t    count; // сколько строк уже
    size_t    capacity; // сколько помещается в память
} LineTable;

static int table_add(LineTable *t, long offset, int length) // одну строку в таблицу
{
    if (t->count == t->capacity) {
        size_t new_cap = (t->capacity == 0) ? 16 : t->capacity * 2;
        LineInfo *p = realloc(t->items, new_cap * sizeof *p);
        if (p == NULL) return -1;
        t->items = p;
        t->capacity = new_cap;
    }
    t->items[t->count].offset = offset;
    t->items[t->count].length = length;
    t->count++;
    return 0;
}

/* Первый проход: отступы и длины всех строк */
static int build_table(int fd, LineTable *t)
{
    char    buf[BLOCK_SIZE];
    ssize_t n;
    long    line_start = 0;
    int     line_len = 0;
    long    file_pos = 0;

    if (lseek(fd, 0L, SEEK_SET) == (off_t)-1) return -1;

    while ((n = read(fd, buf, sizeof buf)) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                if (table_add(t, line_start, line_len) != 0)
                    return -1;
                line_start = file_pos + i + 1;
                line_len = 0;
            } else {
                line_len++;
            }
        }
        file_pos += n;
    }
    if (n < 0) return -1;
    if (line_len > 0 && table_add(t, line_start, line_len) != 0) return -1;
    return 0;
}

static void print_table(const LineTable *t)
{
    size_t i;
    printf("--- Debug: Line Table ---\n");
    for (i = 0; i < t->count; i++)
        printf("Line %zu: Offset = %ld, Length = %d\n",
               i + 1, t->items[i].offset, t->items[i].length);
    printf("-------------------------\n");
}

/* ---------- Асинхронная часть ---------- */
static void dump_file_safe(int fd)
{
    static char buf[BLOCK_SIZE];
    ssize_t n;
    if (lseek(fd, 0L, SEEK_SET) == (off_t)-1) return;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(STDOUT_FILENO, buf + off, (size_t)(n - off));
            if (w <= 0) return;
            off += w;
        }
    }
}

/* Вспомогательная функция для безопасного вывода строки в обработчике */
static void safe_write_str(const char *str) {
    size_t len = strlen(str);
    write(STDOUT_FILENO, str, len);
}

/* Обработчик SIGALRM */
static void alarm_handler(int sig)
{
    (void)sig;
    alarm(0);
    g_timeout_triggered = 1;
    
    safe_write_str("\n[TIMEOUT] Time is up! Printing full file content...\n");
    
    // ИСПРАВЛЕНИЕ: Вычисляем и выводим время чтения прямо здесь через write()
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    
    double elapsed = (end_time.tv_sec - g_start_time.tv_sec) + 
                     (end_time.tv_usec - g_start_time.tv_usec) / 1000000.0;
    
    // Форматируем время в буфер вручную, чтобы избежать printf
    char time_buf[64];
    snprintf(time_buf, sizeof(time_buf), "File reading time: %.6f seconds\n", elapsed);
    safe_write_str(time_buf);
    
    if (g_fd != -1)
        dump_file_safe(g_fd);
}

/* ---------- Обычное чтение выбранной строки ---------- */
static int show_line(int fd, const LineInfo *li)
{
    char *buf = malloc((size_t)li->length + 1);
    int   got = 0;
    if (buf == NULL) return -1;
    if (lseek(fd, (off_t)li->offset, SEEK_SET) == (off_t)-1) {
        free(buf);
        return -1;
    }
    while (got < li->length) {
        ssize_t r = read(fd, buf + got, (size_t)(li->length - got));
        if (r < 0) { free(buf); return -1; }
        if (r == 0) break;
        got += (int)r;
    }
    buf[got] = '\0';
    printf("%s\n", buf);
    free(buf);
    return 0;
}

int main(int argc, char *argv[])
{
    LineTable table = { NULL, 0, 0 };
    int num;
    int first_attempt_done = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    g_fd = open(argv[1], O_RDONLY);
    if (g_fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    //  Запоминаем время начала чтения в глобальную переменную
    gettimeofday(&g_start_time, NULL);

    if (build_table(g_fd, &table) != 0) {
        perror("build_table");
        close(g_fd);
        free(table.items);
        return EXIT_FAILURE;
    }

    print_table(&table);

    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        perror("signal");
        close(g_fd);
        free(table.items);
        return EXIT_FAILURE;
    }

    for (;;) {
        g_timeout_triggered = 0;

        if (!first_attempt_done) {
            printf("Enter line number (0 to quit, %d sec timeout): ", TIMEOUT_SECONDS);
            fflush(stdout);
            alarm(TIMEOUT_SECONDS);
            
            int rc = scanf("%d", &num);
            alarm(0); 
            
            first_attempt_done = 1;

            if (g_timeout_triggered) {
                break; 
            }

            if (rc == EOF) break;
            if (rc != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
                continue;
            }
        } else {
            printf("Enter line number (0 to quit): ");
            fflush(stdout);
            int rc = scanf("%d", &num);
            if (rc == EOF) break;
            if (rc != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
                continue;
            }
        }

        if (num == 0) break;
        
        if (num < 1 || (size_t)num > table.count) {
            printf("No such line: there are %u lines in the file\n",
                   (unsigned)table.count);
            continue;
        }
        
        if (show_line(g_fd, &table.items[num - 1]) != 0) {
            perror("show_line");
            break;
        }
    }

    close(g_fd);
    free(table.items);
    return EXIT_SUCCESS;
}