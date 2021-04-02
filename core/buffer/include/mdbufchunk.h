#ifndef MD_BUF_CHUNK_H
#define MD_BUF_CHUNK_H

#include <stdint.h>
#include <stdbool.h>

#include "mdmetadata.h"

typedef struct {
	int order;
	int size;
	md_metadata_t* metadata;
	bool decoder_done;
	uint8_t chunk[];
} md_buf_chunk_t;

int md_buf_chunk_init(void);
md_buf_chunk_t* md_buf_chunk_append_byte(md_buf_chunk_t*, uint8_t);
void md_buf_chunk_free(md_buf_chunk_t*);
void md_buf_chunk_deinit(void);

#endif
