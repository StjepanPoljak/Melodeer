#include "mdwav.h"
#include "mdcore.h"

bool MDWAV__compress = true;

void MDWAV__error (MD__file_t *MD__file, char *message) {

    printf("Error: %s\n", message);
    MD__decoding_error_signal (MD__file);
    MD__exit_decoder ();
}

void *MDWAV__parse (void *MD__file)
{
    unsigned int channels = 0;
    unsigned int bits = 0;
    unsigned int frequency = 0;

    unsigned char *buffer;
    unsigned int buff_size = MD__get_buffer_size ((MD__file_t *)MD__file);

    buffer = malloc (4);

    FILE *file = ((MD__file_t *)MD__file)->file;

    int temp_read_res = 0;

    // 1-4 RIFF
    temp_read_res = fread (buffer, 1, 4, file);

    if (buffer[0] != 'R'
     || buffer[1] != 'I'
     || buffer[2] != 'F'
     || buffer[3] != 'F') {

        MDWAV__error ((MD__file_t *)MD__file, "No RIFF identifier.");
    }

    // 5-8 size
    temp_read_res = fread (buffer, 1, 4, file);

    int fsize = buffer [3] << 24
              | buffer [2] << 16
              | buffer [1] << 8
              | buffer [0];

    // 9-12 WAVE
    temp_read_res = fread (buffer, 1, 4, file);

    if (buffer[0] != 'W'
     || buffer[1] != 'A'
     || buffer[2] != 'V'
     || buffer[3] != 'E') {

        MDWAV__error ((MD__file_t *)MD__file, "No WAVE identifier.\n");
    }

    // 13-16 'fmt '
    temp_read_res = fread (buffer, 1, 4, file);
    if (buffer [0] != 'f'
     || buffer [1] != 'm'
     || buffer [2] != 't'
     || buffer [3] != ' ') {

        MDWAV__error ((MD__file_t *)MD__file, "Error: No fmt marker.");
    }

    // 17-20 fmt length
    temp_read_res = fread (buffer, 1, 4, file);
    unsigned int fmtlen = buffer [3] << 24
                        | buffer [2] << 16
                        | buffer [1] << 8
                        | buffer [0];

    // 21-22 format type ( 1 = PCM )
    temp_read_res = fread (buffer, 1, 2, file);
    if (buffer[1] != 0
     || buffer[0] != 1) {

        printf ("Error: Unsupported format.\n");
        MD__decoding_error_signal ((MD__file_t *)MD__file);
        MD__exit_decoder ();
    }

    // 23-24 channels
    temp_read_res = fread (buffer, 1, 2, file);
    channels  = buffer [1] << 8
              | buffer [0];

    // 25-28 sample rate
    temp_read_res = fread (buffer, 1, 4, file);
    frequency = buffer [3] << 24
              | buffer [2] << 16
              | buffer [1] << 8
              | buffer [0];

    // 29-32 byte rate
    temp_read_res = fread (buffer, 1, 4, file);

    int block_size = buffer [3] << 24
                   | buffer [2] << 16
                   | buffer [1] << 8
                   | buffer [0];

    // 33-34 block alignment
    temp_read_res = fread (buffer, 1, 2, file);
    int block_align = buffer[1]<<8
                    | buffer[0];

    // 35-36 bits per sample
    temp_read_res = fread (buffer, 1, 2, file);
    bits = buffer [1] << 8
         | buffer [0];

    // if there is offset to data marker, skip it
    if (fmtlen >= 16) {

        fseek (file, fmtlen - 16, SEEK_CUR);
    }

    // next 4 bytes data marker
    temp_read_res = fread (buffer, 1, 4, file);

    if (buffer[0] != 'd'
     || buffer[1] != 'a'
     || buffer[2] != 't'
     || buffer[3] != 'a') {

        printf ("No data marker.\n");
        MD__decoding_error_signal ((MD__file_t *)MD__file);
        MD__exit_decoder ();
    }

    // data size
    temp_read_res = fread (buffer, 1, 4, file);
    unsigned int data_size = buffer [3] << 24
                           | buffer [2] << 16
                           | buffer [1] << 8
                           | buffer [0];

    MD__set_metadata ((MD__file_t *)MD__file, frequency, channels, bits, (data_size / channels * 8) / bits);

    unsigned char *metatemp = buffer;
    unsigned int done_processing = 0;

    while (true) {

        if (done_processing >= data_size || MD__did_stop ((MD__file_t *)MD__file)) break;

        if (bits <= 16 || !MDWAV__compress) {
            // this will be freed by the buffer algorithm, so no worries
            buffer = malloc (buff_size);

            unsigned int read_size = fread (buffer, 1, buff_size, file);

            MD__add_buffer_chunk_ncp ((MD__file_t *)MD__file, buffer, read_size);
            done_processing += read_size;

        } else {

        // TODO: compression for > 16bps

        }
    }

    fclose (file);
    free (metatemp);

    MD__decoding_done_signal ((MD__file_t *)MD__file);
    MD__exit_decoder ();

    return NULL;
}
