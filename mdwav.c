#include "mdwav.h"
#include "mdcore.h"

void MDWAV__error (char *message) {

    printf("Error: %s\n", message);
    MD__decoding_error_signal ();
    MD__exit_decoder ();
}

void *MDWAV__parse (void *filename)
{
    unsigned int channels = 0;
    unsigned int bits = 0;
    unsigned int frequency = 0;

    unsigned char *buffer;
    unsigned int buff_size = MD__get_buffer_size ();
    FILE *f;

    buffer = malloc (4);

    f = fopen ((char *) filename, "rb");

    // 1-4 RIFF
    fread (buffer, 1, 4, f);

    if (buffer[0] != 'R'
     || buffer[1] != 'I'
     || buffer[2] != 'F'
     || buffer[3] != 'F') {

        MDWAV__error ("No RIFF identifier.");
    }

    // 5-8 size
    fread (buffer, 1, 4, f);

    int fsize = buffer [3] << 24
              | buffer [2] << 16
              | buffer [1] << 8
              | buffer [0];

    printf ("File size: %d (%.2f mb)\n\n", fsize, (float)fsize/(1024*1024));

    // 9-12 WAVE
    fread (buffer, 1, 4, f);

    if (buffer[0] != 'W'
     || buffer[1] != 'A'
     || buffer[2] != 'V'
     || buffer[3] != 'E') {

        MDWAV__error ("No WAVE identifier.\n");
    }

    // 13-16 'fmt '
    fread (buffer, 1, 4, f);
    if (buffer [0] != 'f'
     || buffer [1] != 'm'
     || buffer [2] != 't'
     || buffer [3] != ' ') {

        MDWAV__error ("Error: No fmt marker.");
    }

    // 17-20 fmt length
    fread (buffer, 1, 4, f);
    unsigned int fmtlen = buffer [3] << 24
                        | buffer [2] << 16
                        | buffer [1] << 8
                        | buffer [0];

    printf ("Fmt size: %d\n\n", fmtlen);

    // 21-22 format type ( 1 = PCM )
    fread (buffer, 1, 2, f);
    if (buffer[1] != 0
     || buffer[0] != 1) {

        printf ("Error: Unsupported format.\n");
        MD__decoding_error_signal ();
        MD__exit_decoder ();
    }

    // 23-24 channels
    fread (buffer, 1, 2, f);
    channels  = buffer [1] << 8
              | buffer [0];

    printf (" | channels:        %d\n", channels);

    // 25-28 sample rate
    fread (buffer, 1, 4, f);
    frequency = buffer [3] << 24
              | buffer [2] << 16
              | buffer [1] << 8
              | buffer [0];

    printf (" | sample rate:     %d\n", frequency);

    // 29-32 byte rate
    fread (buffer, 1, 4, f);

    int block_size = buffer [3] << 24
                   | buffer [2] << 16
                   | buffer [1] << 8
                   | buffer [0];
    printf (" | block size:      %d\n", block_size);

    // 33-34 block alignment
    fread (buffer, 1, 2, f);
    int block_align = buffer[1]<<8
                    | buffer[0];
    printf(" | block align:     %d\n", block_align);

    // 35-36 bits per sample
    fread (buffer, 1, 2, f);
    bits = buffer [1] << 8
         | buffer [0];

    printf (" | bps:             %d\n", bits);

    // if there is offset to data marker, skip it
    if (fmtlen >= 16) {

        fseek (f, fmtlen - 16, SEEK_CUR);
    }

    // next 4 bytes data marker
    fread (buffer, 1, 4, f);

    if (buffer[0] != 'd'
     || buffer[1] != 'a'
     || buffer[2] != 't'
     || buffer[3] != 'a') {

        printf ("No data marker.\n");
        MD__decoding_error_signal ();
        MD__exit_decoder ();
    }

    // data size
    fread (buffer, 1, 4, f);
    unsigned int data_size = buffer [3] << 24
                           | buffer [2] << 16
                           | buffer [1] << 8
                           | buffer [0];

    MD__set_metadata (frequency, channels, bits, (data_size / channels * 8) / bits);

    unsigned char *metatemp = buffer;
    unsigned int done_processing = 0;

    while (true) {

        if (done_processing >= data_size) break;

        // this will be freed by the buffer algorithm, so no worries
        buffer = malloc (buff_size);

        unsigned int read_size = fread (buffer, 1, buff_size, f);

        MD__add_buffer_chunk_ncp (buffer, read_size);
        done_processing += read_size;
    }

    fclose (f);
    free (metatemp);

    MD__decoding_done_signal ();
    MD__exit_decoder ();
}
