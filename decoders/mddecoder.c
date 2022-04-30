#include "mddecoder.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mdlog.h"
#include "mdbuf.h"
#include "mdsettings.h"
#include "mdcoreops.h"
#include "mddecoderll.h"

#define decoder_data(DATA) \
	((md_decoder_data_t*)DATA)

static int md_decode_as_fp(md_decoder_data_t* decoder_data) {
	FILE* f;

	f = fopen(decoder_data->fpath, "r");
	if (!f) {
		md_error("Could not open file %s.", decoder_data->fpath);
		return -EINVAL;
	}

	decoder_data->data = (void*)f;

	return decoder_data->fdecoder->ops.decode_fp(decoder_data);
}

/*
static void* md_decoder_handler(void* data) {
	md_decoder_t* fdecoder;

	if (fdecoder->ops.decode_bytes) {
		FILE* file;
		md_log("Decoding as bytes...");
		file = fopen(decoder_data(data)->fpath, "r");
		fseek(file, 0L, SEEK_END);
		decoder_data(data)->size = ftell(file);
		decoder_data(data)->pos = 0;
		decoder_data(data)->buff = malloc(decoder_data(data)->size);
		rewind(file);
		decoder_data(data)->size = fread(decoder_data(data)->buff, 1,
						 decoder_data(data)->size,
						 file);
		fclose(file);
		decoder_data(data)->fdecoder->ops.decode_bytes(decoder_data(data));
	}
	else if (fdecoder->ops.decode_fp) {
		md_decode_as_fp(decoder_data(data));
	}

exit_decoder_handler:

	if (decoder_data(data)->force_decoder)
		free(decoder_data(data)->force_decoder);
	free(data);

	return NULL;
}
*/

void md_decoder_init(md_decoder_data_t* decoder_data,
		     const char* fpath, const char* decoder) {

	decoder_data->fpath = strdup(fpath);
	decoder_data->force_decoder = decoder ? strdup(decoder) : NULL;
	decoder_data->chunk = NULL;
	decoder_data->metadata = NULL;
	decoder_data->fdecoder = NULL;
	decoder_data->data = NULL;

	return;
}

int md_decoder_start(md_decoder_data_t* decoder_data) {
	int ret = 0;

	if (decoder_data->force_decoder) {
		if (!(decoder_data->fdecoder = md_decoder_ll_find(
				decoder_data->force_decoder))) {
			md_error("Could not find decoder %s.",
				decoder_data->force_decoder);
			ret = -EINVAL;
			goto exit_decoder;
		}
	}
	else if (!(decoder_data->fdecoder =
			md_decoder_for(decoder_data->fpath))) {
		md_error("Could not find decoder for %s.",
			 decoder_data->fpath);
		ret = -EINVAL;
		goto exit_decoder;
	}

	decoder_data->buff = NULL;

	if (md_decoder_try_load(decoder_data->fdecoder)) {
		md_error("Could not open library for %s decoder.",
			 decoder_data->fdecoder->name);
		ret = -EINVAL;
		goto exit_decoder;
	}

	md_decode_as_fp(decoder_data);

exit_decoder:

	if (decoder_data->force_decoder)
		free(decoder_data->force_decoder);
	free(decoder_data);

	return ret;
}

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

int md_start_take_in(void* data) {

	md_exec_event(first_chunk_take_in, (md_buf_chunk_t*)data);

	return 0;
}

int md_add_decoded_byte(md_decoder_data_t* decoder_data, uint8_t byte) {
	md_buf_chunk_t* old;
	static md_metadata_t* oldmeta = NULL;
	int ret;

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
			md_evq_add_event(old->evq, MD_EVENT_RUN_ON_TAKE_IN,
					 md_start_take_in,
					 (void*)old);
		}

		old->metadata = decoder_data->metadata;

		if (md_adjust_size(old)) {
			md_error("Could not adjust buffer size.");

			return -EINVAL;
		}

		ret = md_buf_add(old);

		return ret == MD_BUF_EXIT ? MD_DEC_EXIT : ret;
	}

	return 0;
}

int md_done_take_in(void* data) {

	((md_buf_chunk_t*)data)->size ^= MD_DECODER_DONE_BIT;
	md_exec_event(last_chunk_take_in, (md_buf_chunk_t*)data);

	return 0;
}

int md_done_take_out(void* data) {

	md_metadata_t* metadata = ((md_buf_chunk_t*)data)->metadata;

	md_exec_event(last_chunk_take_out, (md_buf_chunk_t*)data);

	free(metadata->fname);
	free(metadata);

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
				     MD_EVENT_RUN_ON_TAKE_IN,
				     md_done_take_in,
				     (void*)decoder_data->chunk))) {
			md_error("Could not add decoder done event.");

			return ret;
		}

		if ((ret = md_evq_add_event(decoder_data->chunk->evq,
				     MD_EVENT_RUN_ON_TAKE_OUT,
				     md_done_take_out,
				     (void*)decoder_data->chunk))) {
			md_error("Could not add decoder done event.");

			return ret;
		}

		md_buf_add(decoder_data->chunk);
		decoder_data->chunk = NULL;
	}

	if (decoder_data->buff)
		free(decoder_data->buff);

	return ret;
}

void md_decoder_deinit(void) {

	return;
}

