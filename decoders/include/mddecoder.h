#ifndef MD_DECODER_H
#define MD_DECODER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "mddecoderll.h"
#include "mdmetadata.h"
#include "mdbufchunk.h"
#include "mdsettings.h"

#define MD_DEC_EXIT 1

typedef struct md_decoder_data_t {
	char* fpath;
	char* force_decoder;
	md_decoder_t* fdecoder;
	md_metadata_t* metadata;
	md_buf_chunk_t* chunk;
	void* data;
	pthread_t decoder_thread;

	int pos;
	char* buff;
	int size;
} md_decoder_data_t;

void md_decoder_init(md_decoder_data_t*,
		     const char*, const char*);
int md_decoder_start(md_decoder_data_t*);
void md_decoder_deinit(void);

int md_add_decoded_byte(md_decoder_data_t*, uint8_t);
int md_decoder_done(md_decoder_data_t*);

#define md_metadata_init(METADATA) do {		\
	(METADATA)->fname = NULL;		\
	(METADATA)->sample_rate = 0;		\
	(METADATA)->channels = 0;		\
	(METADATA)->bps = 0;			\
	(METADATA)->total_samples = 0;		\
	(METADATA)->total_buf_chunks = 0;	\
} while (0)

#define md_set_metadata(_decoder_data, _metadata) do {		\
	(_decoder_data)->metadata = (_metadata);		\
	if ((_metadata) && !((_metadata)->total_buf_chunks)) {	\
		(_metadata)->total_buf_chunks =			\
			(((_metadata)->total_samples		\
			* (_metadata)->channels			\
			* ((_metadata)->bps / 8))		\
			/ (get_settings()->buf_size)) + 1;	\
	}							\
} while (0)

#endif
