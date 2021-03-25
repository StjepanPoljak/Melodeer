#ifndef MD_BUF_H
#define MD_BUF_H

#include "mdbufchunk.h"
#include "mdsettings.h"

struct md_buf_pack_t {
	const md_buf_chunk_t*(*first)(const struct md_buf_pack_t*);
	const md_buf_chunk_t*(*next)(const struct md_buf_pack_t*);
	void* data;
};

typedef struct md_buf_pack_t md_buf_pack_t;

#define get_pack_data(pack, type) ((type*)((md_buf_pack_t*)pack)->data)

int md_buf_init(const md_settings_t*);
int md_buf_add(md_buf_chunk_t*);
int md_buf_get(const md_buf_chunk_t**);
int md_buf_get_pack(md_buf_pack_t**, int*);
int md_buf_deinit(void);

#endif
