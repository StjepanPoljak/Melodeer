#include "mdcore.h"

#include <string.h>

#include "mdlog.h"
#include "mdsettings.h"
#include "mdbuf.h"
#include "mddecoder.h"

int md_init(md_core_ops_t* md_core_ops) {
	int ret;

	if ((ret = load_settings())) {
		md_error("Error loading settings.");
		return ret;
	}

	md_set_core_ops(md_core_ops);

	md_buf_init();
	md_driver_init();
	md_decoder_init();

	return 0;
}

int md_play_async(const char* fname, const char* decoder) {

	return md_decoder_start(fname, decoder, MD_ASYNC_DECODER);
}

void md_stop(void) {

	md_stop_decoder_engine();
	md_buf_flush();
	md_driver_stop();

	return;
}

void md_resume(void) {

	md_buf_resume();
	md_driver_resume();
	md_start_decoder_engine();

	return;
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

void md_deinit(bool early) {

	if (!early) {
		md_buf_deinit();
		md_driver_deinit();
	}

	md_decoder_ll_deinit();
	md_driver_ll_deinit();

	if (!early)
		md_decoder_deinit();

	return;
}
