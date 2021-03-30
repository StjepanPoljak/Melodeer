#include "mddriver.h"

#include <stdlib.h>
#include <string.h>

#include "mdlog.h"
#include "mdll.h"
#include "mdsettings.h"
#include "mdcoreops.h"

static md_driver_ll* md_driverll_head;
static md_metadata_t* curr_metadata;

#define get_driver_ops() get_settings()->driver->ops

int md_driver_exec_events(md_buf_chunk_t* chunk) {
	int ret;

	ret = 0;

	if (chunk->metadata && (chunk->metadata != curr_metadata)) {
		ret = get_driver_ops().set_metadata(chunk->metadata);
		if (ret) {
			md_error("Error executing set_metadata().");
			return ret;
		}
		md_exec_event(loaded_metadata, chunk, chunk->metadata);
		curr_metadata = chunk->metadata;
	}

	if (chunk->decoder_done)
		md_exec_event(done_playing_file, chunk);

	return ret;
}

md_driver_ll* md_driver_ll_add(md_driver_t* driver) {
	md_driver_ll* last;
	md_driver_ll* new;

	new = malloc(sizeof(*new));
	if (!new) {
		md_error("Could not allocate memory.\n");
		return NULL;
	}

	new->driver = driver;
	new->next = NULL;

	__ll_add(md_driverll_head, new, last);

	return new;
}

md_driver_t* md_driver_ll_find(const char* name) {
	md_driver_ll* curr;

	__ll_find(md_driverll_head, name, driver, curr);

	return curr->driver;
}

static void md_driver_ll_free(md_driver_ll* curr) {
	md_driver_ll* temp;

	temp = curr;
	free(temp);

	return;
}

int md_driver_ll_deinit(void) {
	md_driver_ll* curr;
	md_driver_ll* next;

	__ll_deinit(md_driverll_head,
		    md_driver_ll_free,
		    curr, next);

	return 0;
}

