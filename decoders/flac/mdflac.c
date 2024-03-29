#include "mddecoder.h"

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "FLAC/stream_decoder.h"
#include "mdlog.h"
#include "mdbufchunk.h"
#include "mdutils.h"

#define FLAC_SYMBOLS(FUN)					\
	FUN(FLAC__stream_decoder_init_FILE);			\
	FUN(FLAC__stream_decoder_init_stream);			\
	FUN(FLAC__stream_decoder_process_until_end_of_stream);	\
	FUN(FLAC__stream_decoder_finish);			\
	FUN(FLAC__stream_decoder_delete);			\
	FUN(FLAC__stream_decoder_new);

typedef struct {
	FILE* file;
	int ret;
} md_flac_decoder_data_t;

FLAC_SYMBOLS(md_define_fptr);

#define md_flac_load_sym(_func) \
	md_load_sym(_func, &(flac_decoder), &(error))

static md_decoder_t flac_decoder;

int md_flac_load_symbols(void) {
	int error;

	if (!flac_decoder.handle) {
		md_error("OpenAL library hasn't been opened.");

		return -EINVAL;
	}

	error = 0;

	FLAC_SYMBOLS(md_flac_load_sym);

	if (error) {
		md_error("Could not load symbols.");

		return -EINVAL;
	}

	return 0;
}

#define FLAC__stream_decoder_init_FILE FLAC__stream_decoder_init_FILE_ptr
#define FLAC__stream_decoder_init_stream FLAC__stream_decoder_init_stream_ptr
#define FLAC__stream_decoder_process_until_end_of_stream \
	FLAC__stream_decoder_process_until_end_of_stream_ptr
#define FLAC__stream_decoder_finish FLAC__stream_decoder_finish_ptr
#define FLAC__stream_decoder_delete FLAC__stream_decoder_delete_ptr
#define FLAC__stream_decoder_new FLAC__stream_decoder_new_ptr

void md_flac_metadata_cb(const FLAC__StreamDecoder* decoder,
			 const FLAC__StreamMetadata* metadata,
			 void* client_data) {
	md_metadata_t* meta;

	meta = malloc(sizeof(*meta));
	if (!meta) {
		md_error("Could not allocate memory.");
		goto early_fail_metadata;
	}

	md_metadata_init(meta);

	meta->fname = ((md_decoder_data_t*)client_data)->fpath;
	meta->sample_rate = metadata->data.stream_info.sample_rate;
	meta->channels = metadata->data.stream_info.channels;
	meta->bps = metadata->data.stream_info.bits_per_sample;
	meta->total_samples = metadata->data.stream_info.total_samples;

	if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
		md_error("Metadata not valid.");
		goto fail_metadata;
	}

	md_set_metadata((md_decoder_data_t*)client_data, meta);

	return;

fail_metadata:
	free(meta);

early_fail_metadata:
	md_decoder_done((md_decoder_data_t*)client_data);

	return;
}

static void md_flac_error_cb(const FLAC__StreamDecoder* decoder,
			     FLAC__StreamDecoderErrorStatus status,
			     void* client_data) {

	md_error("Error decoding FLAC data.");
}

static int process_block(const FLAC__Frame* frame,
			 const FLAC__int32* const buffer[],
			 int i, md_decoder_data_t* decoder_data) {
	int c, b, ret;

	for (c = 0; c < frame->header.channels; c++) {

		for (b = 0; b < frame->header.bits_per_sample; b += 8) {

			ret = md_add_decoded_byte(decoder_data,
						  buffer[c][i] >> b);
			if (ret)
				return ret;
		}
	}

	return 0;
}

#define set_flac_ret(DEC_DATA, RET) do {				\
	((md_flac_decoder_data_t*)((DEC_DATA)->data))->ret = RET;	\
} while (0)

static FLAC__StreamDecoderWriteStatus md_flac_write_cb(
			const FLAC__StreamDecoder* decoder,
			const FLAC__Frame* frame,
			const FLAC__int32* const buffer[],
			void* client_data) {
	int i, ret;

	for (i = 0; i < frame->header.blocksize; i++) {

		ret = process_block(frame, buffer, i,
				   (md_decoder_data_t*)client_data);
		if (ret)
			goto abort_flac_decoder;

	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

abort_flac_decoder:

	if (ret == MD_DEC_EXIT) {
		md_log("Aborting FLAC decoder.");
		set_flac_ret((md_decoder_data_t*)client_data, MD_DEC_EXIT);
	}
	else {
		md_error("Error processing block.");
		set_flac_ret((md_decoder_data_t*)client_data, ret);
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
}

FLAC__StreamDecoderReadStatus md_flac_read_cb(
					const FLAC__StreamDecoder* decoder,
					FLAC__byte buffer[], size_t* bytes,
					void* client_data) {
	md_decoder_data_t* decoder_data = (md_decoder_data_t*)client_data;
	if (*bytes <= 0)
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	else if (*bytes + decoder_data->pos > decoder_data->size) {
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}
	else {
		memcpy(buffer, &(decoder_data->buff[decoder_data->pos]),
		       *bytes);
		decoder_data->pos += *bytes;
		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}
}

static int md_flac_decode_raw(md_decoder_data_t* decoder_data, bool decode_fp) {
	FLAC__bool ok;
	FLAC__StreamDecoderInitStatus init_status;
	FLAC__StreamDecoder* md_flac_decoder;
	md_flac_decoder_data_t* flac_data;
	FILE* file;
	int ret;

	ret = 0;
	ok = true;

	flac_data = malloc(sizeof(*flac_data));
	if (!flac_data) {
		md_error("Error allocating memory.");
		ret = -ENOMEM;
		goto exit_decoder;
	}

	flac_data->ret = 0;
	file = (FILE*)decoder_data->data;
	decoder_data->data = (void*)flac_data;

	if (!(md_flac_decoder = FLAC__stream_decoder_new())) {
		md_error("Could not create FLAC decoder.");
		ret = -EINVAL;
		goto exit_decoder;
	}

	if (decode_fp) {
		init_status = FLAC__stream_decoder_init_FILE(
				md_flac_decoder,
				file,
				md_flac_write_cb,
				md_flac_metadata_cb,
				md_flac_error_cb,
				(void*)decoder_data
				);
	}
	else {
		init_status = FLAC__stream_decoder_init_stream(
				md_flac_decoder,
				md_flac_read_cb,
				NULL, NULL, NULL, NULL,
				md_flac_write_cb,
				md_flac_metadata_cb,
				md_flac_error_cb,
				(void*)decoder_data
				);
	}

	ok = init_status == FLAC__STREAM_DECODER_INIT_STATUS_OK;

	if (ok)
		ok = FLAC__stream_decoder_process_until_end_of_stream(
				md_flac_decoder
			);

	FLAC__stream_decoder_finish(md_flac_decoder);
	FLAC__stream_decoder_delete(md_flac_decoder);

	if (!ok && (flac_data->ret != MD_DEC_EXIT)) {
		md_error("Could not process file.");
		ret = -EINVAL;
		goto exit_decoder;
	}

exit_decoder:
	if (flac_data)
		free(flac_data);
	md_decoder_done(decoder_data);

	return ret;
}

static int md_flac_decode_fp(md_decoder_data_t* decoder_data) {

	return md_flac_decode_raw(decoder_data, true);
}

static int md_flac_decode_bytes(md_decoder_data_t* decoder_data) {

	return md_flac_decode_raw(decoder_data, false);
}

bool md_flac_decodes_file(const char* file) {

	return !strcmp(md_extension_of(file), "flac");
}

static md_decoder_t flac_decoder = {
	.name = "flac",
	.lib = "libFLAC.so",
	.handle = NULL,
	.ops = {
		.load_symbols = md_flac_load_symbols,
		.decodes_file = md_flac_decodes_file,
		.decode_file = NULL,
		.decode_fp = md_flac_decode_fp,
		.decode_bytes = md_flac_decode_bytes,
	}
};

register_decoder(flac, flac_decoder);
