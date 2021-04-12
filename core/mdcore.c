#include "mdcore.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "mdlog.h"
#include "mdsettings.h"
#include "mdbuf.h"
#include "mddecoder.h"
#include "mdcoreops.h"
#include "mdgarbage.h"

static bool volatile md_running = true;

void md_loaded_metadata(md_buf_chunk_t* chunk, md_metadata_t* metadata) {

	md_log("Loaded metadata (ch=%d, bps=%d, rate=%d)",
	       metadata->channels, metadata->bps, metadata->sample_rate);

	return;
}

void md_will_load_last_chunk(md_buf_chunk_t* chunk) {

	md_log("Done playing file.");

	md_running = false;

	return;
}

static md_core_ops_t md_core_ops = {
	.will_load_chunk = NULL,
	.loaded_metadata = md_loaded_metadata,
	.will_load_last_chunk = md_will_load_last_chunk
};

int md_init(void) {

	md_set_core_ops(&md_core_ops);
	if (load_settings()) {
		md_error("Error loading settings.");
		return -EINVAL;
	}
	md_buf_init();
	get_settings()->driver->ops.init();

	md_decoder_start("/home/stjepan/Develop/Melodeer/03 - Scarified.flac", NULL, MD_ASYNC_DECODER);
//	md_decoder_start("CMakeLists.txt", "dummy", MD_ASYNC_DECODER);

	while (md_running) { }

	md_deinit();

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

void md_deinit(void) {

	md_buf_deinit();
	get_settings()->driver->ops.deinit();
	md_driver_unload(get_settings()->driver);
	md_garbage_deinit();

	md_decoder_ll_deinit();
	md_driver_ll_deinit();

	return;
}
