#include "mdbufchunk.h"

#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"
#include "mdsettings.h"

int md_buf_chunk_init(void) {

	return 0;
}

static md_buf_chunk_t* md_buf_chunk_new(int order) {
	md_buf_chunk_t* curr;

	curr = malloc(sizeof(*curr)
		    + sizeof(*curr->chunk) * get_settings()->buf_size);
	if (!curr) {
		md_error("Could not allocate memory.");
		return NULL;
	}

	curr->evq = malloc(sizeof(*curr->evq));
	if (!curr->evq) {
		md_error("Could not allocate memory for event queue.");
		return NULL;
	}

	md_evq_init(curr->evq);

	curr->order = order;
	curr->size = 0;
	curr->metadata = NULL;

	return curr;
}

md_buf_chunk_t* md_buf_chunk_append_byte(md_buf_chunk_t* buf_chunk,
					 uint8_t byte) {
	md_buf_chunk_t* curr;
	int order;

	order = buf_chunk ? buf_chunk->order + 1 : 0;

	if (!buf_chunk || (buf_chunk->size >= get_settings()->buf_size)) {

		if (!(curr = md_buf_chunk_new(order))) {
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
	md_evq_deinit(temp->evq);
	free(temp->evq);
	free(temp);

	return;
}

void md_buf_chunk_deinit(void) {

	return;
}
