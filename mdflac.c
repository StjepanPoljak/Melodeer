#include "mdflac.h"
#include "mdcore.h"

void *MDFLAC__start_decoding (void *filename)
{
    FLAC__bool                      ok = true;
    FLAC__StreamDecoder             *decoder = 0;
    FLAC__StreamDecoderInitStatus   init_status;

    if ((decoder = FLAC__stream_decoder_new ()) == NULL) {

        MD__decoding_error_signal ();
    }

    init_status = FLAC__stream_decoder_init_file (decoder,
                                                  (char *) filename,
                                                  MDFLAC__write_callback,
                                                  MDFLAC__metadata_callback,
                                                  MDFLAC__error_callback,
                                                  NULL);

    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {

        ok = false;
    }

    if (ok) {

        ok = FLAC__stream_decoder_process_until_end_of_stream (decoder);
    }

    FLAC__stream_decoder_delete(decoder);

    MD__decoding_done_signal();

    if (!ok) {

        MD__decoding_error_signal();
    }

    MD__exit_decoder();

}

void MDFLAC__metadata_callback (const FLAC__StreamDecoder   *decoder,
                                const FLAC__StreamMetadata  *metadata,
                                void                        *client_data)
{
	(void) decoder, (void) client_data;

	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {

        MD__set_metadata (metadata->data.stream_info.sample_rate,
                          metadata->data.stream_info.channels,
                          metadata->data.stream_info.bits_per_sample,
                          metadata->data.stream_info.total_samples);
	}
}

void MDFLAC__error_callback (const FLAC__StreamDecoder          *decoder,
                             FLAC__StreamDecoderErrorStatus     status,
                             void                               *client_data) {

	(void)decoder, (void)client_data;

    MD__decoding_error_signal();

	printf("Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__StreamDecoderWriteStatus MDFLAC__write_callback (const FLAC__StreamDecoder     *decoder,
                                                              const FLAC__Frame             *frame,
                                                              const FLAC__int32 *const      buffer[],
                                                              void                          *client_data) {
    MD__lock ();
    for (unsigned int i = 0; i < frame->header.blocksize; i++)
    {
        for (int j=3; j>=0; j--)
        {
            unsigned int temp_buffer = buffer [j/2][i];
            if (j%2 == 0)
            {
                temp_buffer = temp_buffer>>8;
            }
            MD__add_to_buffer_raw ((unsigned char)temp_buffer);
        }
    }
    MD__unlock ();

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
