#include "mddecoder.h"

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"
#include "mdutils.h"

static md_decoder_t dummy_decoder;

static int md_dummy_set_metadata(md_decoder_data_t* decoder_data) {
	md_metadata_t* meta;

	meta = malloc(sizeof(*meta));
	if (!meta) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	meta->sample_rate = 0;
	meta->channels = 0;
	meta->bps = 0;
	meta->total_buf_chunks = 0;

	md_set_metadata(decoder_data, meta);

	return 0;
}

int md_dummy_decode_fp(md_decoder_data_t* decoder_data) {
	uint8_t buf;
	int ret;
	FILE* file;

	file = (FILE*)(decoder_data->data);

	if ((ret = md_dummy_set_metadata(decoder_data))) {
		md_error("Could not set metadata.");
		return ret;
	}

	while (fread(&buf, 1, 1, file), !feof(file)) {
		ret = md_add_decoded_byte(decoder_data, buf);
		if (ret)
			goto abort_decoder;
	}

	fclose(file);

	md_decoder_done(decoder_data);

	return 0;

abort_decoder:
	if (ret == MD_DEC_EXIT)
		md_log("Aborting decoder...");
	else
		md_error("Error adding decoded byte.");

	fclose(file);

	return ret;
}

bool md_dummy_decodes_file(const char* file) {

	return !strcmp(md_extension_of(file), "txt");
}

static md_decoder_t dummy_decoder = {
	.name = "dummy",
	.lib = NULL,
	.handle = NULL,
	.ops = {
		.decodes_file = md_dummy_decodes_file,
		.decode_file = NULL,
		.decode_fp = md_dummy_decode_fp,
		.decode_bytes = NULL,
	}
};

register_decoder(dummy, dummy_decoder);
