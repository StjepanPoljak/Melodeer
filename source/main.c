#include <stdlib.h>
#include <stdio.h>

#include "mdcore.h"
#include "mdflac.h"
#include "mdwav.h"

enum MD__filetype { MD__FLAC, MD__WAV, MD__UNKNOWN };

typedef enum MD__filetype MD__filetype;

MD__filetype MD__get_extension (const char *filename);

void MD__handle_metadata (unsigned int sample_rate,
                          unsigned int channels,
                          unsigned int bps,
                          unsigned int total_samples);

int main (int argc, char *argv[])
{
    MD__initialize ();
    MDAL__initialize ();

    void *(* decoder)(void *) = NULL;

    MD__filetype type = MD__get_extension (argv[1]);

    switch (type) {

        case MD__FLAC:
            decoder = MDFLAC__start_decoding;
            break;

        case MD__WAV:
            decoder = MDWAV__parse;
            break;

        default:
            printf("Unknown file type.\n");
            break;
    }

    if (decoder == NULL) exit (1);

    MD__play (argv[1], decoder, MD__handle_metadata);

    MDAL__close ();

    exit(0);
}

MD__filetype MD__get_extension (const char *filename) {

    char curr = filename [0];
    unsigned int last_dot_position = -1;
    unsigned int i = 0;

    while (curr != 0) {

        if (curr == '.') {

            last_dot_position = i;
        }

        curr = filename [i++];
    }

    if (last_dot_position == -1) return MD__UNKNOWN;

    unsigned int diff = i - last_dot_position;

    if (diff == 5) {

        if (filename [last_dot_position]     == 'f'
         && filename [last_dot_position + 1] == 'l'
         && filename [last_dot_position + 2] == 'a'
         && filename [last_dot_position + 3] == 'c') {

            return MD__FLAC;
        }
    }

    if (diff == 4) {

        if (filename [last_dot_position]     == 'w'
         && filename [last_dot_position + 1] == 'a'
         && filename [last_dot_position + 2] == 'v') {

            return MD__WAV;
        }
    }

    return MD__UNKNOWN;
}

void MD__handle_metadata (unsigned int sample_rate,
                          unsigned int channels,
                          unsigned int bps,
                          unsigned int total_samples) {

    unsigned int total_seconds  = total_samples / sample_rate;
    unsigned int hours          = total_seconds / 3600;
    unsigned int minutes        = (total_seconds / 60) - (hours * 60);
    unsigned int seconds        = total_seconds - 60 * minutes;

    unsigned int data_size      = total_samples * (bps / 8) * channels;
    float kb                    = data_size / 1024;
    float mb                    = kb / 1024;
    bool data_test              = mb >= 1.0;

    printf("\n");
    printf(" | sample rate      : %d Hz\n", sample_rate);
    printf(" | channels         : %s\n", (channels == 2) ? "Stereo" : "Mono");
    printf(" | quality          : %d bps\n", bps);
    printf(" | data size        : %.2f %s\n", data_test ? mb : kb, data_test ? "Mb" : "Kb");
    printf(" | duration         : ");
    if (hours > 0)      printf("%dh ", hours);
    if (minutes > 0)    printf("%dm ", minutes);
    if (seconds > 0)    printf ("%ds", seconds);
    printf("\n");

    printf("\n");
}
