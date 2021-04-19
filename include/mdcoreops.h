#ifndef MD_CORE_OPS_H
#define MD_CORE_OPS_H

#include "mdbufchunk.h"
#include "mdmetadata.h"

typedef struct {
	void(*loaded_metadata)(md_buf_chunk_t*, md_metadata_t*);
	void(*will_load_chunk)(md_buf_chunk_t*);
	void(*last_chunk_take_in)(md_buf_chunk_t*);
	void(*last_chunk_take_out)(md_buf_chunk_t*);
	void(*melodeer_stopped)(void);
} md_core_ops_t;

void md_set_core_ops(md_core_ops_t*);
md_core_ops_t* md_get_core_ops(void);

#define md_exec_event(ev_name, ...) ({				\
	if (md_get_core_ops() && md_get_core_ops()->ev_name)	\
		md_get_core_ops()->ev_name(__VA_ARGS__);	\
})

#endif
