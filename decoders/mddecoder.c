#include "mddecoder.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "mdlog.h"
#include "mdbuf.h"
#include "mdsettings.h"
#include "mdcoreops.h"

#define decoder_data(DATA) \
	((md_decoder_data_t*)DATA)

static md_decoder_ll* md_decoderll_head;
static pthread_t decoder_thread;

DEFINE_SYM_FUNCTIONS(decoder);

static int md_decode_as_fp(md_decoder_data_t* decoder_data) {
	FILE* f;

	f = fopen(decoder_data->fpath, "rw");
	if (!f) {
		md_error("Could not open file %s.", decoder_data->fpath);
		return -EINVAL;
	}

	decoder_data->data = (void*)f;

	return decoder_data->fdecoder->ops.decode_fp(decoder_data);
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

	decoder_data(data)->fdecoder = fdecoder;

	if (md_decoder_try_load(fdecoder)) {
		md_error("Could not open library for %s decoder.",
			 fdecoder->name);
		goto exit_decoder_handler;
	}

	if (fdecoder->ops.decode_fp)
		md_decode_as_fp(decoder_data(data));

exit_decoder_handler:
	free(decoder_data(data)->fpath);
	if (decoder_data(data)->force_decoder)
		free(decoder_data(data)->force_decoder);
	free(data);

	return NULL;
}

int md_decoder_start(const char* fpath, const char* decoder,
		     md_decoding_mode_t decoder_mode) {
	int ret;
	md_decoder_data_t* decoder_data;

	if ((decoder_mode != MD_BLOCKING_DECODER)
	 && (decoder_mode != MD_ASYNC_DECODER)) {
		md_error("Invalid decoding mode: %d", decoder_mode);
		return -EINVAL;
	}

	decoder_data = malloc(sizeof(*decoder_data));
	if (!decoder_data) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	decoder_data->fpath = strdup(fpath);
	decoder_data->force_decoder = decoder ? strdup(decoder) : NULL;
	decoder_data->chunk = NULL;
	decoder_data->metadata = NULL;
	decoder_data->fdecoder = NULL;
	decoder_data->data = NULL;

	ret = 0;

	if ((ret = pthread_create(&decoder_thread, NULL,
				  md_decoder_handler,
				  (void*)decoder_data))) {
		md_error("Could not create decoder thread.");
		return ret;
	}

	switch (decoder_mode) {
	case MD_BLOCKING_DECODER:
		if ((ret = pthread_join(decoder_thread, NULL))) {
			md_error("Could not join thread.");
			return ret;
		}
		break;
	case MD_ASYNC_DECODER:
		if ((ret = pthread_detach(decoder_thread))) {
			md_error("Could not detach thread.");
			return ret;
		}
		break;
	default:
		md_error("Internal bug: should never be here.");
		return -EINVAL;
	}

	return ret;
}

#define MD_BUFFER_PADDING 4

static int md_adjust_size(md_buf_chunk_t* chunk) {
	int i, size_remainder;

	size_remainder = chunk->size % MD_BUFFER_PADDING;

	if (size_remainder) {
		if (get_settings()->buf_size - chunk->size < size_remainder) {
			md_error("BUG: Inconsistent buffer size.");
			return -EINVAL;
		}
		for (i = 0; i < MD_BUFFER_PADDING - size_remainder; i++)
			chunk->chunk[chunk->size++] = 0;
	}

	return 0;
}

int md_add_decoded_byte(md_decoder_data_t* decoder_data, uint8_t byte) {
	md_buf_chunk_t* old;
	static md_metadata_t* oldmeta = NULL;

	old = decoder_data->chunk;

	decoder_data->chunk = md_buf_chunk_append_byte(decoder_data->chunk,
						       byte);
	if (!decoder_data->chunk) {
		md_error("Could not append byte.");

		return -ENOMEM;
	}
	else if (old && (decoder_data->chunk != old)) {

		if (oldmeta != decoder_data->metadata) {
			md_exec_event(loaded_metadata, decoder_data->chunk,
				      decoder_data->metadata);
			oldmeta = decoder_data->metadata;
		}

		old->metadata = decoder_data->metadata;

		if (md_adjust_size(old)) {
			md_error("Could not adjust buffer size.");

			return -EINVAL;
		}

		return md_buf_add(old);
	}

	return 0;
}

int md_exec_on_done(void* data) {

	md_buf_chunk_t* chunk;

	chunk = (md_buf_chunk_t*)data;

	chunk->size ^= MD_DECODER_DONE_BIT;

	md_exec_event(will_load_last_chunk, chunk);
	free(chunk->metadata);

	return 0;
}

int md_decoder_done(md_decoder_data_t* decoder_data) {
	int ret;

	ret = 0;

	if (decoder_data->chunk) {
		decoder_data->chunk->metadata = decoder_data->metadata;

		if ((ret = md_adjust_size(decoder_data->chunk))) {
			md_error("Could not adjust buffer size.");

			return ret;
		}

		md_set_decoder_done(decoder_data->chunk);

		if ((ret = md_evq_add_event(decoder_data->chunk->evq,
				     MD_EVENT_RUN_ON_TAKE_IN, md_exec_on_done,
				     (void*)decoder_data->chunk))) {
			md_error("Could not add decoder done event.");

			return ret;
		}

		md_buf_add(decoder_data->chunk);
		decoder_data->chunk = NULL;
	}

	return ret;
}

void md_decoder_deinit(void) {

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

