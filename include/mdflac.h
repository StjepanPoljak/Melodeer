#ifndef MDFLAC_H

#include "FLAC/stream_decoder.h"

void *MDFLAC__start_decoding (void *MD__file);

static FLAC__StreamDecoderWriteStatus MDFLAC__write_callback (const FLAC__StreamDecoder     *decoder,
                                                              const FLAC__Frame             *frame,
                                                              const FLAC__int32 *const      buffer[],
                                                              void                          *client_data);

static void MDFLAC__metadata_callback (const FLAC__StreamDecoder  *decoder,
                                       const FLAC__StreamMetadata *metadata,
                                       void                       *client_data);

static void MDFLAC__error_callback (const FLAC__StreamDecoder         *decoder,
                                    FLAC__StreamDecoderErrorStatus    status,
                                    void                              *client_data);

#endif

#define MDFLAC_H
