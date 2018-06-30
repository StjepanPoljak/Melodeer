#include <stdlib.h>
#include <stdio.h>

#include "mdcore.h"
#include "mdflac.h"
#include "mdwav.h"

enum MD__filetype { MD__FLAC, MD__WAV, MD__UNKNOWN };

typedef enum MD__filetype MD__filetype;

MD__filetype MD__get_extension (const char *filename);

int main (int argc, char *argv[])
{
    MD__initialize ();
    MDAL__initialize ();

    void *(* decoder)(void *) = NULL;

    MD__filetype type = MD__get_extension (argv[1]);

    switch (type) {

        case MD__FLAC:
            printf("Playing flac.\n");
            decoder = MDFLAC__start_decoding;
            break;

        case MD__WAV:
            printf("Playing wav.\n");
            decoder = MDWAV__parse;
            break;

        default:
            printf("Unknown file type.\n");
            break;
    }

    if (decoder == NULL) exit (1);

    MD__play (argv[1], decoder);

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

        if (filename[last_dot_position] == 'f'
        && filename[last_dot_position + 1] == 'l'
        && filename[last_dot_position + 2] == 'a'
        && filename[last_dot_position + 3] == 'c') {

            return MD__FLAC;
        }
    }

    if (diff == 4) {

        if (filename[last_dot_position] == 'w'
        && filename[last_dot_position + 1] == 'a'
        && filename[last_dot_position + 2] == 'v') {

            return MD__WAV;
        }
    }

    return MD__UNKNOWN;

}
