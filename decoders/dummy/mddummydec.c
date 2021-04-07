#include "mddecoder.h"

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"
#include "mdbufchunk.h"
#include "mdutils.h"

static md_decoder_t dummy_decoder;

static int md_dummy_set_metadata(void) {
	md_metadata_t* meta;

	meta = malloc(sizeof(*meta));
	if (!meta) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	meta->sample_rate = 0;
	meta->channels = 0;
	meta->bps = 0;
	meta->total_samples = 0;

	if (md_set_metadata(meta)) {
		md_error("Invalid metadata.");
		return -EINVAL;
	}

	return 0;
}

int md_dummy_decode_fp(FILE* file) {
	uint8_t buf;
	int ret;

	if ((ret = md_dummy_set_metadata())) {
		md_error("Could not set metadata.");
		return ret;
	}

	while (fread(&buf, 1, 1, file), !feof(file))
		ret = md_add_decoded_byte(&dummy_decoder, buf);
		if (ret) {
			md_error("Error adding decoded byte.");
			return ret;
		}

	fclose(file);

	md_decoder_done(&dummy_decoder);

	return 0;
}

bool md_dummy_decodes_file(const char* file) {

	return !strcmp(md_extension_of(file), "txt");
}

static md_decoder_t dummy_decoder = {
	.name = "dummy",
	.chunk = NULL,
	.ops = {
		.decodes_file = md_dummy_decodes_file,
		.decode_file = NULL,
		.decode_fp = md_dummy_decode_fp,
		.decode_bytes = NULL,
	}
};

register_decoder(dummy, dummy_decoder);
