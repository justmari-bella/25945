#include <stdio.h>      /* printf, scanf, fprintf, perror */
#include <stdlib.h>     /* malloc, realloc, free, EXIT_*  */
#include <unistd.h>     /* read, lseek, close             */
#include <fcntl.h>      /* open, O_RDONLY, SEEK_SET, SEEK_CUR */

#define BLOCK_SIZE 4096   /* размер блока для чтения файла */

/* Одна запись таблицы: где строка начинается и сколько в ней байт */
typedef struct {
    long offset;          /* смещение начала строки от начала файла */
    int  length;          /* длина строки в байтах (без '\n')        */
} LineInfo;

/* Таблица строк с динамическим расширением */
typedef struct {
    LineInfo *items;      /* массив записей                          */
    size_t    count;      /* количество заполненных записей          */
    size_t    capacity;   /* выделенная ёмкость массива              */
} LineTable;

/* Добавить запись в таблицу; 0 — успех, -1 — нет памяти */
static int table_add(LineTable *t, long offset, int length)
{
    if (t->count == t->capacity) {                      /* таблица полна — расширяем вдвое */
        size_t new_cap = (t->capacity == 0) ? 16 : t->capacity * 2;
        LineInfo *p = realloc(t->items, new_cap * sizeof *p);
        if (p == NULL)
            return -1;                                  /* не удалось выделить память */
        t->items = p;
        t->capacity = new_cap;
    }
    t->items[t->count].offset = offset;                 /* заполняем новую запись */
    t->items[t->count].length = length;
    t->count++;                                         /* увеличиваем счётчик */
    return 0;
}

/* Первый проход: строим таблицу смещений и длин строк */
static int build_table(int fd, LineTable *t)
{
    char buf[BLOCK_SIZE];
    ssize_t n;
    long line_start = 0;
    int line_len = 0;

    while ((n = read(fd, buf, sizeof buf)) > 0) {
        /* Позиция сразу после прочитанного блока */
        long current_pos = (long)lseek(fd, 0L, SEEK_CUR);

        if (current_pos == -1)
            return -1;

        /* Начало текущего блока */
        long base = current_pos - n;

        for (size_t i = 0; i < (size_t)n; i++) {
            if (buf[i] == '\n') {
                /* Сохраняем найденную строку, включая пустую */
                if (table_add(t, line_start, line_len) != 0)
                    return -1;

                /* Начало следующей строки */
                line_start = base + (long)i + 1;
                line_len = 0;
            } else {
                line_len++;
            }
        }
    }

    /* Проверяем ошибку чтения */
    if (n < 0)
        return -1;

    /* Добавляем последнюю строку, если файл не заканчивается на '\n' */
    if (line_len > 0) {
        if (table_add(t, line_start, line_len) != 0)
            return -1;
    }

    return 0;
}

/* Отладочная печать всей таблицы */
static void print_table(const LineTable *t)
{
    size_t i;
    printf("--- Debug: Line Table ---\n");
    for (i = 0; i < t->count; i++)
        printf("Line %u: Offset = %ld, Length = %d\n",
               (unsigned)(i + 1), t->items[i].offset, t->items[i].length);
    printf("-------------------------\n");
}

/* Прочитать и напечатать одну строку по её записи в таблице */
static int show_line(int fd, const LineInfo *li)
{
    char *buf = malloc((size_t)li->length + 1);       /* +1 байт под завершающий '\0' */
    int   got = 0;
    if (buf == NULL)
        return -1;                                    /* нет памяти */

    if (lseek(fd, (off_t)li->offset, SEEK_SET) == (off_t)-1) {  /* переход к началу строки */
        free(buf);
        return -1;
    }

    while (got < li->length) {                        /* читаем ровно length байт */
        ssize_t r = read(fd, buf + got, (size_t)(li->length - got));
        if (r < 0) {
            free(buf);
            return -1;                                /* ошибка чтения */
        }
        if (r == 0)                                   /* файл короче ожидаемого */
            break;
        got += (int)r;
    }
    buf[got] = '\0';                                  /* read() НЕ ставит '\0' сам — делаем вручную */
    printf("%s\n", buf);                              /* печатаем содержимое строки */
    free(buf);
    return 0;
}

int main(int argc, char *argv[])
{
    LineTable table = { NULL, 0, 0 };                 /* пустая таблица перед началом работы */
    int fd, num;

    if (argc != 2) {                                  /* нужен один аргумент — имя файла */
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);                     /* открываем файл только для чтения */
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    if (build_table(fd, &table) != 0) {               /* первый проход: строим таблицу */
        perror("build_table");
        close(fd);
        free(table.items);                            /* освобождаем частично выделенную память */
        return EXIT_FAILURE;
    }

    print_table(&table);                              /* сверяем с таблицей, посчитанной вручную */

    for (;;) {                                        /* интерактивный цикл запросов */
        printf("Enter line number (0 to quit): ");
        fflush(stdout);                               /* принудительно выводим приглашение */
        if (scanf("%d", &num) != 1)                   /* не число или EOF — выходим */
            break;
        if (num == 0)                                 /* ноль — завершение программы */
            break;
        if (num < 1 || (size_t)num > table.count) {   /* проверка диапазона */
            printf("No such line: there are %u lines in the file\n",
                   (unsigned)table.count);
            continue;
        }
        /* нумерация для пользователя с 1, индекс массива — с 0 */
        if (show_line(fd, &table.items[num - 1]) != 0) {
            perror("show_line");
            break;
        }
    }

    close(fd);                                        /* закрываем файловый дескриптор */
    free(table.items);                                /* освобождаем память таблицы */
    return EXIT_SUCCESS;
}