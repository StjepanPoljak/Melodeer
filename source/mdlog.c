#include "mdlog.h"

volatile bool MDLOG__reset_signal = true;

void MD__log_internal (const char *string) {

    FILE *f = fopen ("mdcore.log", "a");

    if (f == NULL) return;

    time_t t = time (NULL);
    struct tm tm = *localtime (&t);

    fprintf (f, "[%02d.%02d.%04d. %02d:%02d:%02d] : %s\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                                                           tm.tm_hour, tm.tm_min, tm.tm_sec, string);

    fclose (f);

    return;
}

void MD__log (const char *string, ...) {

    va_list list;

    va_start (list, string);
    unsigned int string_size = vsnprintf(NULL, 0, string, list) + 1;
    va_end (list);
    char *buffer = malloc (sizeof(buffer)*string_size);

    va_start (list, string);
    vsprintf (buffer, string, list);
    va_end (list);

    MD__log_internal (buffer);

    MDLOG__dynamic_reset ();

    free (buffer);
}

void MDLOG__dynamic_reset () {

    MDLOG__reset_signal = true;
}

void MDLOG__dynamic (const char *string, ...) {

    static volatile bool first = true;
    static char buffer [1000];

    va_list list;

    va_start (list, string);
    vsprintf (buffer, string, list);
    va_end (list);

    if (MDLOG__reset_signal) {

        MD__log_internal (buffer);

        MDLOG__reset_signal = false;
        first = true;
    }
    else first = false;
}

void MDLOG__reset_log () {

    FILE *f = fopen ("mdcore.log", "w");

    if (f == NULL) return;

    fprintf (f, "");

    fclose (f);

    return;
}
