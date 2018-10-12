#include "mdmpg123.h"

void *MDMPG123__decoder (void *MD__file) {

    mpg123_init();

    int err;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;

    int channels, encoding;
    long rate;
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char*)malloc(buffer_size * sizeof(unsigned char));

    mpg123_open(mh, ((MD__file_t *) MD__file)->filename);
    mpg123_getformat(mh, &rate, &channels, &encoding);

    MD__set_metadata ((MD__file_t *)MD__file, (int) rate, channels, MPG123_SAMPLESIZE (encoding) * 8, 0);

    unsigned int counter = 0;

    for (int totalBtyes = 0; mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK; ) {

        short* tst = (short *)buffer;
        for (int i = 0; i < buffer_size; i++) {

            MD__add_to_buffer ((MD__file_t *)MD__file, ((unsigned char)buffer[i]));
        }
        counter += buffer_size;
        totalBtyes += done;
    }

    MD__decoding_done_signal ((MD__file_t *)MD__file);

    free(buffer);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    MD__exit_decoder ();

    return NULL;
}
