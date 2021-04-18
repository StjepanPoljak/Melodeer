#include "mdsettings.h"

#include <stdlib.h>
#include <errno.h>

#include "mdlog.h"

static md_settings_t settings = {
	.buf_size = 4096,
	.buf_num = 4,
	.min_buf_num = 3,
	.max_buf_num = 5,
	.driver = NULL
};

int load_settings(void) {
	const char* driver_name = "openal";

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
