#include "mdcoreops.h"

#include <stdlib.h>

void md_will_load_chunk_default(void* data, md_buf_chunk_t* chunk) {
	(void)data, (void)chunk;

	return;
}

static md_core_ops_t* md_core_ops;

static md_core_ops_t md_core_ops_default = {
	.loaded_metadata = NULL,
	.will_load_chunk = md_will_load_chunk_default,
	.last_chunk_take_in = NULL,
	.last_chunk_take_out = NULL,
	.stopped = NULL,
	.paused = NULL,
	.playing = NULL,
	.driver_error = NULL,
	.buffer_underrun = NULL,
	.data = NULL
};

void md_set_core_ops(md_core_ops_t* core_ops) {

	md_core_ops = core_ops;

	if (!md_core_ops)
		md_core_ops = &md_core_ops_default;

	else if (!md_core_ops->will_load_chunk)
		md_core_ops->will_load_chunk = md_will_load_chunk_default;

	return;
}

md_core_ops_t* md_get_core_ops(void) {

	return md_core_ops;
}
