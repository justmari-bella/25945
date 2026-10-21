#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main(void){
    time_t now;
    // Указатель на структуру с датой и временем
    struct tm *sp;

    // Устанавливаем часовой пояс Калифорнии.
    // Он автоматически учитывает PST (зимой) и PDT (летом).
    setenv("TZ", "America/Los_Angeles", 1);

    // Применяем установленный часовой пояс
    tzset();

    // Получаем текущее системное время
    time(&now);

    // Преобразуем время в местное время Калифорнии
    sp = localtime(&now);

    // Выводим дату, время и название часового пояса.
    // tm_mon начинается с 0, поэтому прибавляем 1.
    // tm_year считает годы с 1900, поэтому прибавляем 1900.
    // tm_isdst определяет, действует ли летнее время.
    printf("%02d/%02d/%04d %02d:%02d %s\n",
           sp->tm_mon + 1,
           sp->tm_mday,
           sp->tm_year + 1900,
           sp->tm_hour,
           sp->tm_min,
           tzname[sp->tm_isdst]);

    return 0;
}
