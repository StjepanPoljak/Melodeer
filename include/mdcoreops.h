#ifndef MD_CORE_OPS_H
#define MD_CORE_OPS_H

#include "mdbufchunk.h"
#include "mdmetadata.h"

typedef struct {
	void(*loaded_metadata)(void*, md_buf_chunk_t*, md_metadata_t*);
	void(*will_load_chunk)(void*, md_buf_chunk_t*);
	void(*first_chunk_take_in)(void*, md_buf_chunk_t*);
	void(*last_chunk_take_in)(void*, md_buf_chunk_t*);
	void(*last_chunk_take_out)(void*, md_buf_chunk_t*);
	void(*stopped)(void*);
	void(*paused)(void*);
	void(*playing)(void*);
	void(*driver_error)(void*);
	void(*buffer_underrun)(void*, bool);
	void* data;
} md_core_ops_t;

void md_set_core_ops(md_core_ops_t*);
md_core_ops_t* md_get_core_ops(void);

#define md_will_load_chunk_exec(CHUNK) do {				\
	md_get_core_ops()->will_load_chunk(md_get_core_ops()->data,	\
					   CHUNK);			\
} while (0)

#define md_exec_event(ev_name, ...) do {				\
	if (md_get_core_ops()->ev_name)					\
		md_get_core_ops()->ev_name(md_get_core_ops()->data,	\
					   ##__VA_ARGS__);		\
} while (0)

#endif
