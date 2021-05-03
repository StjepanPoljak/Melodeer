#include "mdsettings.h"

#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"

static md_settings_t settings = {
	.buf_size = 512,
	.buf_num = 4,
	.max_buf_num = 16,
	.buf_underrun = {
		.count = 5,
		.secs = 1
	},
	.driver = NULL,
	.verbose = true
};

int load_settings(void) {
	const char* driver_name = "pulseaudio";

	settings.driver = md_driver_ll_find(driver_name);

	if (!settings.driver) {
		md_error("Could not load driver %s.", driver_name);
		return -EINVAL;
	}

	return md_driver_try_load(settings.driver);
}

md_settings_t* get_settings(void) {

	return &settings;
}
