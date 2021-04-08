#include "mddecoder.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "mdlog.h"
#include "mdbuf.h"
#include "mdsettings.h"

#define decoder_data(DATA) \
	((md_decoder_data_t*)DATA)

typedef struct {
	char* fpath;
	char* force_decoder;
} md_decoder_data_t;

static md_decoder_ll* md_decoderll_head;
static md_metadata_t* curr_metadata;
static pthread_t decoder_thread;

static int md_decode_as_fp(const char* fpath,
			   md_decoder_t* fdecoder) {
	FILE* f;

	f = fopen(fpath, "rw");
	if (!f) {
		md_error("Could not open file %s.", fpath);
		return -EINVAL;
	}

	return fdecoder->ops.decode_fp(f);
}

static md_decoder_t* md_decoder_ll_find(const char* name) {
	md_decoder_ll* curr;

	__ll_find(md_decoderll_head, name, decoder, curr);

	return curr ? curr->decoder : NULL;
}

static void* md_decoder_handler(void* data) {
	md_decoder_t* fdecoder;

	if (decoder_data(data)->force_decoder) {
		if (!(fdecoder = md_decoder_ll_find(
				decoder_data(data)->force_decoder))) {
			md_error("Could not find decoder %s.",
				decoder_data(data)->force_decoder);
			goto exit_decoder_handler;
		}
	}
	else if (!(fdecoder = md_decoder_for(decoder_data(data)->fpath))) {
		md_error("Could not find decoder for %s.",
			 decoder_data(data)->fpath);
		goto exit_decoder_handler;
	}

	if (fdecoder->ops.decode_fp)
		md_decode_as_fp(decoder_data(data)->fpath, fdecoder);

exit_decoder_handler:
	free(decoder_data(data)->fpath);
	if (decoder_data(data)->force_decoder)
		free(decoder_data(data)->force_decoder);
	free(data);

	return NULL;
}

int md_decoder_start(const char* fpath, const char* decoder) {
	int ret;
	md_decoder_data_t* decoder_data;

	decoder_data = malloc(sizeof(*decoder_data));
	if (!decoder_data) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	decoder_data->fpath = strdup(fpath);
	decoder_data->force_decoder = decoder ? strdup(decoder) : NULL;

	ret = 0;

	if ((ret = pthread_create(&decoder_thread, NULL,
				  md_decoder_handler,
				  (void*)decoder_data))) {
		md_error("Could not create decoder thread.");
		return ret;
	}

	return ret;
}

int md_set_metadata(md_metadata_t* metadata) {

	curr_metadata = metadata;

	return 0;
}

int md_add_decoded_byte(md_decoder_t* decoder, uint8_t byte) {
	md_buf_chunk_t* old;

	old = decoder->chunk;

	decoder->chunk = md_buf_chunk_append_byte(decoder->chunk, byte);
	if (!decoder->chunk)

		return ({ md_error("Could not append byte."); }), -ENOMEM;

	else if (old && (decoder->chunk != old))

		return old->metadata = curr_metadata, md_buf_add(old);

	return 0;
}

void md_decoder_done(md_decoder_t* decoder) {

	if (decoder->chunk) {
		decoder->chunk->decoder_done = true;
		decoder->chunk->metadata = curr_metadata;
		md_buf_add(decoder->chunk);
		decoder->chunk = NULL;
	}

	return;
}

void md_decoder_deinit(void) {

	//pthread_join(decoder_thread, NULL);

	return;
}

md_decoder_ll* md_decoder_ll_add(md_decoder_t* decoder) {
	md_decoder_ll* last;
	md_decoder_ll* new;

	new = malloc(sizeof(*new));
	if (!new) {
		md_error("Could not allocate memory.");
		return NULL;
	}

	new->decoder = decoder;
	new->next = NULL;

	__ll_add(md_decoderll_head, new, last);

	return new;
}

md_decoder_t* md_decoder_for(const char* fpath) {
	md_decoder_ll* curr;

	__ll_for_each(md_decoderll_head, curr) {
		if (curr->decoder->ops.decodes_file(fpath))
			return curr->decoder;
	}

	return NULL;
}

static void md_decoder_ll_free(md_decoder_ll* curr) {
	md_decoder_ll* temp;

	temp = curr;
	free(temp);

	return;
}

int md_decoder_ll_deinit(void) {
	md_decoder_ll* curr;
	md_decoder_ll* next;

	__ll_deinit(md_decoderll_head,
		    md_decoder_ll_free,
		    curr, next);

	return 0;
}

