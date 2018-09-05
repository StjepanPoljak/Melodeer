#include <stdlib.h>
#include <stdio.h>

#include "mdcore.h"
#include "mdflac.h"
#include "mdwav.h"
#include "mdlame.h"

enum MD__filetype { MD__FLAC, MD__WAV, MD__MP3, MD__UNKNOWN };

typedef enum MD__filetype MD__filetype;

MD__filetype MD__get_extension (const char *filename);

void MD__handle_metadata (MD__metadata_t metadata);
unsigned int MD__get_seconds (volatile MD__buffer_chunk_t *curr_chunk,
                              unsigned int sample_rate,
                              unsigned int channels,
                              unsigned int bps);

void transform (volatile MD__buffer_chunk_t *curr_chunk,
                unsigned int sample_rate,
                unsigned int channels,
                unsigned int bps);

void MD__handle_errors (char *info);

void MD__completion () {

    printf("Done playing!\n");
}

void MD__playing_handle () {

    printf("Playing!\n");
}

int main (int argc, char *argv[])
{
    MDAL__initialize (4096, 4, 4);

    void *(* decoder)(void *) = NULL;

    MD__file_t MD__file;

    MD__file.MD__buffer_transform = transform;

    for (int i=1; i<argc; i++) {

        printf("\nLoading: %s\n", argv[i]);

        if (MD__initialize (&MD__file, argv[i])) {

            MD__filetype type = MD__get_extension (argv[i]);

            switch (type) {

                case MD__FLAC:
                    decoder = MDFLAC__start_decoding;
                    break;

                case MD__WAV:
                    decoder = MDWAV__parse;
                    break;

                case MD__MP3:
                    decoder = MDLAME__decoder;
                    break;

                default:
                    printf ("Unknown file type.\n");
                    break;
                }

            MD__play (&MD__file, decoder, MD__handle_metadata,
                      MD__playing_handle, MD__handle_errors, MD__completion);
        }
    }

    MDAL__close ();

    return 0;
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

        if (filename [last_dot_position]     == 'm'
         && filename [last_dot_position + 1] == 'p'
         && filename [last_dot_position + 2] == '3') {

            return MD__MP3;
        }
    }

    return MD__UNKNOWN;
}

void MD__handle_errors (char *info) {
    printf("(!) %s\n",info);
}

void MD__handle_metadata (MD__metadata_t metadata) {

    unsigned int total_seconds  = metadata.total_samples / metadata.sample_rate;
    unsigned int hours          = total_seconds / 3600;
    unsigned int minutes        = (total_seconds / 60) - (hours * 60);
    unsigned int seconds        = total_seconds - 60 * minutes;

    unsigned int data_size      = metadata.total_samples * (metadata.bps / 8) * metadata.channels;
    float kb                    = data_size / 1024;
    float mb                    = kb / 1024;
    bool data_test              = mb >= 1.0;

    printf("\n");
    printf(" | sample rate      : %d Hz\n", metadata.sample_rate);
    printf(" | channels         : %s\n", (metadata.channels == 2) ? "Stereo" : "Mono");
    printf(" | quality          : %d bps\n", metadata.bps);
    printf(" | data size        : %.2f %s\n", data_test ? mb : kb, data_test ? "Mb" : "Kb");
    printf(" | duration         : ");
    if (hours > 0)      printf("%dh ", hours);
    if (minutes > 0)    printf("%dm ", minutes);
    if (seconds > 0)    printf ("%ds", seconds);
    printf("\n");

    printf("\n");
}

void transform (volatile MD__buffer_chunk_t *curr_chunk,
                unsigned int sample_rate,
                unsigned int channels,
                unsigned int bps) {

    for (int i=0; i<curr_chunk->size/((bps/8)*channels); i++) {

        for (int c=0; c<channels; c++) {

            // this will depend on bps...
            short data = 0;

            for (int b=0; b<bps/8; b++) {

                data = data + ((short)(curr_chunk->chunk[i*channels*(bps/8)+(c*channels)+b])<<(8*b));
            }

            // transform

            for (int b=0; b<bps/8; b++) {

                curr_chunk->chunk[i*channels*(bps/8)+(c*channels)+b] = data >> 8*b;
            }
        }
    }
}

unsigned int MD__get_seconds (volatile MD__buffer_chunk_t *curr_chunk,
                              unsigned int sample_rate,
                              unsigned int channels,
                              unsigned int bps) {

    return (curr_chunk->order * curr_chunk->size / (bps * channels / 8)) / sample_rate;

}
