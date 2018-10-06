#include "mdlog.h"

void MD__log (char *string, ...) {

    FILE *f;
    f = fopen ("mdcore.log", "a");
    if (f == NULL) return;

    time_t t = time (NULL);
    struct tm tm = *localtime (&t);

    fprintf (f, "[%02d.%02d.%04d. %02d:%02d:%02d] : ", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                                                       tm.tm_hour, tm.tm_min, tm.tm_sec, string);
    va_list list;

    vfprintf (f, string, list);
    fprintf (f, "\n");

    fclose (f);

    return;
}

void MDLOG__reset_log () {

    FILE *f;
    f = fopen ("mdcore.log", "w");

    if (f == NULL) return;

    fprintf (f, "");

    fclose (f);

    return;
}

void MDLOG__remove_last_line () {

    FILE *f;
    f = fopen ("mdcore.log", "rw");

    if (f == NULL) return;

    fseek (f, 0, SEEK_END);

    int file_size = ftell (f);

    char buffer [file_size];

    fseek (f, 0, SEEK_SET);

    int i = 0;

    char curr = -1;

    while ((curr = getc (f)) != EOF) buffer[i++] = curr;

    int last_encounter = -1;

    for (int j = i-1; j >= 0; j--)

        if (buffer[j] == '\n') {

            if (last_encounter >= 0 && last_encounter - j > 0) break;

            last_encounter = j;
        }

    if (last_encounter < 0) {

        fclose (f);
        return;
    }

    buffer[last_encounter + 1] = 0;

    fprintf (f, "%s", &buffer);

    fclose (f);

    return;
}
