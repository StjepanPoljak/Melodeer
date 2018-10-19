#include "mdmpg123.h"

void *MDMPG123__decoder (void *data) {

    MD__file_t *MD__file = (MD__file_t *)data;

    mpg123_init();

    int err;
    mpg123_handle *mh = mpg123_new (NULL, &err);
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;

    int channels, encoding;
    long rate;
    buffer_size = mpg123_outblock (mh);
    buffer = malloc (buffer_size * sizeof (*buffer));

    if (mpg123_open (mh, MD__file->filename) != MPG123_OK) {

        MD__decoding_error_signal (MD__file);

        mpg123_delete (mh);
        mpg123_exit ();

        MD__exit_decoder ();
    }

    mpg123_getformat (mh, &rate, &channels, &encoding);

    MD__set_metadata (MD__file, (int) rate, channels, MPG123_SAMPLESIZE (encoding) * 8, 0);

    unsigned int counter = 0;
    
    int totalBtyes = 0;

    while (true) {

    MD__lock (MD__file);

    if (MD__file->MD__stop_playing) {
        
        MD__unlock (MD__file);

        break;
    }

    if (mpg123_read (mh, buffer, buffer_size, &done) != MPG123_OK) {
        
        MD__unlock (MD__file);

        break;
    }

    MD__unlock (MD__file);

        short* tst = (short *)buffer;

        for (int i = 0; i < buffer_size; i++) {

            if (!MD__add_to_buffer (MD__file, ((unsigned char) buffer[i]))) break;
        }
        counter += buffer_size;
        totalBtyes += done;
    }

    MD__decoding_done_signal (MD__file);

    free (buffer);
    mpg123_close (mh);
    mpg123_delete (mh);
    mpg123_exit ();

    MD__exit_decoder ();

    return NULL;
}
