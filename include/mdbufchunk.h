#ifndef MD_BUF_CHUNK_H
#define MD_BUF_CHUNK_H

#include <stdint.h>

#include "mdsettings.h"

typedef struct {
	int order;
	int size;
	uint8_t chunk[1];
} md_buf_chunk_t;

int buf_chunk_init(md_settings_t*);
int buf_chunk_append(uint8_t);
void buf_chunk_free(md_buf_chunk_t*);
void buf_chunk_deinit(void);

#endif
