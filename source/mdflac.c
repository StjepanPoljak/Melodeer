#include "mdflac.h"
#include "mdcore.h"

FLAC__StreamDecoder     *MDFLAC__decoder    = 0;

void *MDFLAC__start_decoding (void *filename)
{
    FLAC__bool                      ok = true;

    FLAC__StreamDecoderInitStatus   init_status;

    if ((MDFLAC__decoder = FLAC__stream_decoder_new ()) == NULL) {

        MD__decoding_error_signal ();
    }

    init_status = FLAC__stream_decoder_init_file (MDFLAC__decoder,
                                                  (char *) filename,
                                                  MDFLAC__write_callback,
                                                  MDFLAC__metadata_callback,
                                                  MDFLAC__error_callback,
                                                  NULL);

    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {

        ok = false;
    }

    if (ok) {

        ok = FLAC__stream_decoder_process_until_end_of_stream (MDFLAC__decoder);
    }

    FLAC__stream_decoder_finish(MDFLAC__decoder);
    FLAC__stream_decoder_delete(MDFLAC__decoder);

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

        bool meta_ok = MD__set_metadata (metadata->data.stream_info.sample_rate,
                                         metadata->data.stream_info.channels,
                                         metadata->data.stream_info.bits_per_sample,
                                         metadata->data.stream_info.total_samples);
        if (!meta_ok) {

            MD__decoding_error_signal ();
            FLAC__stream_decoder_finish (MDFLAC__decoder);
            FLAC__stream_decoder_delete (MDFLAC__decoder);
            MD__exit_decoder ();
        }
    }
}

void MDFLAC__error_callback (const FLAC__StreamDecoder          *MDFLAC__decoder,
                             FLAC__StreamDecoderErrorStatus     status,
                             void                               *client_data) {

	(void)MDFLAC__decoder, (void)client_data;

    MD__decoding_error_signal ();

	printf("Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__StreamDecoderWriteStatus MDFLAC__write_callback (const FLAC__StreamDecoder     *MDFLAC__decoder,
                                                              const FLAC__Frame             *frame,
                                                              const FLAC__int32 *const      buffer[],
                                                              void                          *client_data) {
    unsigned int bps_supp = 16;

    unsigned int bps_mult = ((frame->header.bits_per_sample < bps_supp) ? frame->header.bits_per_sample : bps_supp) / 8;

    unsigned int compress = 0;

    if (frame->header.bits_per_sample == 24) {

        compress = 1;

    } else if (frame->header.bits_per_sample == 32) {

        compress = 2;
    }

    MD__lock ();
    for (unsigned int i = 0; i < frame->header.blocksize; i++) {

        for (int c = 0; c < frame->header.channels; c++) {

            for (int b = 0; b < bps_mult; b++ ) {

                MD__add_to_buffer_raw ((unsigned char)((buffer[c][i] >> (8 * compress))>>(8*b)));
            }

        }
    }
    MD__unlock ();

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}