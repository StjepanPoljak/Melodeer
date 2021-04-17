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

	if (chunk->decoder_done) {
		md_exec_event(will_load_last_chunk, chunk);
		free(chunk->metadata);
	}

	return ret;
}

md_driver_ll* md_driver_ll_add(md_driver_t* driver) {
	md_driver_ll* last;
	md_driver_ll* new;

	new = malloc(sizeof(*new));
	if (!new) {
		md_error("Could not allocate memory.");
		return NULL;
	}

	new->driver = driver;
	new->next = NULL;

	__ll_add(md_driverll_head, new, last);

	return new;
}
/*
void md_driver_unload(md_driver_t* driver) {

	if (driver->handle)
		dlclose(driver->handle);

	return;
}
*/
/*
int md_driver_try_load(md_driver_t* driver) {

	if (!driver->lib)
		return 0;

	if (!driver->handle) {
		driver->handle = dlopen(driver->lib, RTLD_NOW);
		if (!driver->handle) {
			md_error("Could not find library %s for driver %s.",
				 driver->lib, driver->name);

			return -EINVAL;
		}
	}

	if (driver->ops.load_symbols()) {
		md_error("Could not load %s symbols.", driver->name);
		md_driver_unload(driver);

		return -EINVAL;
	}

	return 0;
}
*/

DEFINE_SYM_FUNCTIONS(driver);

md_driver_t* md_driver_ll_find(const char* name) {
	md_driver_ll* curr;

	__ll_find(md_driverll_head, name, driver, curr);

	return curr ? curr->driver : NULL;
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

