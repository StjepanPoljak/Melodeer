#include "mdsettings.h"

#include <stdlib.h>

static md_settings_t settings = {
	.buf_size = 256,
	.buf_num = 4,
	.min_buf_num = 3,
	.driver = NULL
};

void load_settings(void) {

	settings.driver = md_driver_ll_find("openal");

	return;
}

md_settings_t* get_settings(void) {

	return &settings;
}
