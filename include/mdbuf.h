#ifndef MD_BUF_H
#define MD_BUF_H

#include "mdbufchunk.h"
#include "mdsettings.h"

#define MD_BUF_EXIT 1
#define MD_BUF_EXACT_NO_MORE 2

struct md_buf_pack_t {
	md_buf_chunk_t*(*first)(const struct md_buf_pack_t*);
	md_buf_chunk_t*(*next)(const struct md_buf_pack_t*);
	void* data;
	int count;
};

typedef struct md_buf_pack_t md_buf_pack_t;

typedef enum {
	MD_PACK_EXACT,
	MD_PACK_ANY
} md_pack_mode_t;

#define get_pack_data(pack, type) ((type*)((md_buf_pack_t*)pack)->data)

void md_decoder_started(void);
void md_decoder_finished(void);

int md_buf_init(void);
int md_buf_add(md_buf_chunk_t*);
int md_buf_get(const md_buf_chunk_t**);
int md_buf_get_pack(md_buf_pack_t**, int*, md_pack_mode_t);
int md_buf_deinit(void);

#endif
