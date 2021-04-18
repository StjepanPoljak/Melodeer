#ifndef MD_BUF_CHUNK_H
#define MD_BUF_CHUNK_H

#include <stdint.h>
#include <stdbool.h>

#include "mdmetadata.h"
#include "mdevq.h"

#define MD_DECODER_DONE_BIT	1
#define MD_BLEND_BIT		2

#define md_set_decoder_done(_chunk) do {	\
	(_chunk)->size |= MD_DECODER_DONE_BIT;	\
} while (0)

#define md_is_decoder_done(_chunk)		\
	((_chunk)->size & MD_DECODER_DONE_BIT)

typedef struct {
	int order;
	int size;
	md_metadata_t* metadata;
	md_evq_t* evq;
	uint8_t chunk[];
} md_buf_chunk_t;

int md_buf_chunk_init(void);
md_buf_chunk_t* md_buf_chunk_append_byte(md_buf_chunk_t*, uint8_t);
void md_buf_chunk_free(md_buf_chunk_t*);
void md_buf_chunk_deinit(void);

#endif
