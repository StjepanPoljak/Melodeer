#include "mdbufchunk.h"

#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"
#include "mdsettings.h"

int md_buf_chunk_init(void) {

	return 0;
}

static md_buf_chunk_t* md_buf_chunk_new(void) {
	md_buf_chunk_t* curr;

	curr = malloc(sizeof(*curr)
		    + sizeof(*curr->chunk) * get_settings()->buf_size);
	if (!curr) {
		md_error("Could not allocate memory.");
		return NULL;
	}

	curr->order = 0;
	curr->size = 0;
	curr->metadata = NULL;
	curr->decoder_done = false;

	return curr;
}

md_buf_chunk_t* md_buf_chunk_append_byte(md_buf_chunk_t* buf_chunk,
					 uint8_t byte) {
	md_buf_chunk_t* curr;

	if (!buf_chunk || (buf_chunk->size >= get_settings()->buf_size)) {

		if (!(curr = md_buf_chunk_new())) {
			md_error("Could not create new chunk.");
			return NULL;
		}
	}
	else
		curr = buf_chunk;

	curr->chunk[curr->size++] = byte;

	return curr;
}

void md_buf_chunk_free(md_buf_chunk_t* buf_chunk) {
	md_buf_chunk_t* temp;

	temp = buf_chunk;
	free(temp);

	return;
}

void md_buf_chunk_deinit(void) {

	return;
}
