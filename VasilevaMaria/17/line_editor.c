
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define MAX_INPUT 4096
#define LINE_WIDTH 40

static struct termios original_term;
static int terminal_changed = 0;

static void restore_terminal(void)
{
    if (terminal_changed) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_term);
        terminal_changed = 0;
    }
}

static int enable_raw_mode(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &original_term) == -1) {
        return -1;
    }

    raw = original_term;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    terminal_changed = 1;
    if (atexit(restore_terminal) != 0) {
        restore_terminal();
        return -1;
    }

    return 0;
}

/*
 * Печатает буфер с переносом по словам.
 * Возвращает количество выведенных строк.
 */
static int draw_text(const char *buf, size_t len)
{
    size_t i = 0;
    int col = 0;
    int rows = 1;

    while (i < len) {
        if (isspace((unsigned char)buf[i])) {
            char c = buf[i++];

            if (c == '\n') {
                putchar('\n');
                col = 0;
                rows++;
            } else if (col >= LINE_WIDTH) {
                putchar('\n');
                col = 0;
                rows++;
            } else {
                putchar(c);
                col++;
            }
            continue;
        }

        /* Находим длину следующего слова. */
        size_t start = i;
        while (i < len &&
               !isspace((unsigned char)buf[i])) {
            i++;
        }
        size_t word_len = i - start;

        if (col > 0 && col + (int)word_len > LINE_WIDTH) {
            putchar('\n');
            col = 0;
            rows++;
        }

        /*
         * Слово длиннее строки физически невозможно поместить
         * в 40 столбцов целиком, поэтому печатаем его частями.
         */
        for (size_t j = start; j < i; j++) {
            if (col >= LINE_WIDTH) {
                putchar('\n');
                col = 0;
                rows++;
            }
            putchar(buf[j]);
            col++;
        }
    }

    fflush(stdout);
    return rows;
}

static void redraw(const char *buf, size_t len, int *old_rows)
{
    /*
     * Возвращаемся к началу ранее нарисованной области
     * и очищаем строки, которые могли остаться от старого текста.
     */
    for (int i = 0; i < *old_rows; i++) {
        printf("\r\033[2K");
        if (i + 1 < *old_rows) {
            printf("\033[1A");
        }
    }

    printf("\r");
    printf("\033[J");

    *old_rows = draw_text(buf, len);
}

int main(void)
{
    char buffer[MAX_INPUT];
    size_t pos = 0;
    int old_rows = 1;
    unsigned char c;
    unsigned char erase_char;
    unsigned char kill_char;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Run this program in a terminal.\n");
        return EXIT_FAILURE;
    }

    if (enable_raw_mode() == -1) {
        perror("terminal setup");
        return EXIT_FAILURE;
    }

    erase_char = (unsigned char)original_term.c_cc[VERASE];
    kill_char = (unsigned char)original_term.c_cc[VKILL];

    printf("Line editor (Ctrl+D on an empty line to exit)\n");
    printf("> ");
    fflush(stdout);

    for (;;) {
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return EXIT_FAILURE;
        }

        if (n == 0) {
            break;
        }

        /* Ctrl+D завершает программу только на пустой строке. */
        if (c == 4 && pos == 0) {
            printf("\r\nExiting...\r\n");
            break;
        }

        if (c == '\r' || c == '\n') {
            buffer[pos] = '\0';
            printf("\r\n");
            pos = 0;
            old_rows = 1;
            printf("> ");
            fflush(stdout);
            continue;
        }

        /* ERASE — обычно Backspace или Delete. */
        if (c == erase_char || c == 8 || c == 127) {
            if (pos > 0) {
                pos--;
                redraw(buffer, pos, &old_rows);
            } else {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        /* KILL — обычно Ctrl+U. */
        if (c == kill_char) {
            pos = 0;
            redraw(buffer, pos, &old_rows);
            continue;
        }

        /* Ctrl+W: удалить слово и пробелы перед ним. */
        if (c == 23) {
            while (pos > 0 &&
                   isspace((unsigned char)buffer[pos - 1])) {
                pos--;
            }
            while (pos > 0 &&
                   !isspace((unsigned char)buffer[pos - 1])) {
                pos--;
            }
            redraw(buffer, pos, &old_rows);
            continue;
        }

        if (isprint(c) || c == '\t') {
            if (pos < MAX_INPUT - 1) {
                buffer[pos++] = (char)c;
                redraw(buffer, pos, &old_rows);
            } else {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        /* Другие непечатаемые символы: звуковой сигнал. */
        putchar('\a');
        fflush(stdout);
    }

    restore_terminal();
    return EXIT_SUCCESS;
}
