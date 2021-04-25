#include "mddriver.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "mdlog.h"
#include "mdll.h"
#include "mdsettings.h"
#include "mdcoreops.h"

static md_driver_ll* md_driverll_head;


#define get_driver_ops() get_settings()->driver->ops

DEFINE_SYM_FUNCTIONS(driver);

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

int md_driver_toggle_pause(void) {


}
