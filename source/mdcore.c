#include "mdcore.h"

#include <string.h>

#include "mdsettings.h"

int md_init(void) {

	load_settings();

	get_settings()->driver->ops.init();

	get_settings()->driver->ops.deinit();

	return 0;
}

int md_set_metadata(md_metadata_t* metadata) {

	get_settings()->driver->ops.set_metadata(metadata);

	return 0;
}

char* md_get_logo(void) {
	const char* logo_data =
			"\\          /\n"
			" |__    __|\n"
			"   _\\/\\/_\n"
			"   \\_  _/\n"
			"   | /\\ |\n"
			" ~melodeer~\n"
			"    ||||";

	return strdup(logo_data);
}
