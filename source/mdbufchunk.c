#include "mdbufchunk.h"

#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"

extern int md_buf_add(md_buf_chunk_t*);

md_buf_chunk_t* curr;
int chunk_size;

static int buf_chunk_new(void) {

	curr = malloc(sizeof(*curr) + chunk_size - 1);
	if (!curr) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	curr->order = 0;
	curr->size = 0;

	return 0;
}

int buf_chunk_init(md_settings_t* settings) {

	curr = NULL;
	chunk_size = settings->buf_size;

	return 0;
}

int buf_chunk_append(uint8_t byte) {
	int ret;

	if (curr->size >= chunk_size) {

	 	if ((ret = md_buf_add(curr))) {
			md_error("Could not add chunk to buffer.");
			return ret;
		}

		if ((ret = buf_chunk_new())) {
			md_error("Could not create new chunk.");
			return ret;
		}
	}

	curr->chunk[curr->size] = byte;

	return 0;
}

void buf_chunk_free(md_buf_chunk_t* buf_chunk) {
	md_buf_chunk_t* temp;

	temp = buf_chunk;
	free(temp);

	return;
}

void buf_chunk_deinit(void) {

	return;
}
