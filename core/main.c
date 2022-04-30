#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>

#include "mdcore.h"
#include "mdcoreops.h"
#include "mdlog.h"
#include "mdtime.h"
#include "mdplaylist.h"

static void sigint_handler(int signal) {
	(void)signal;

	md_stop();

	return;
}

/*
static void md_loaded_metadata(void* data, md_buf_chunk_t* chunk,
			md_metadata_t* metadata) {

	md_log("Loaded metadata (ch=%d, bps=%d, rate=%d)",
	       metadata->channels, metadata->bps, metadata->sample_rate);

	return;
}

static void md_last_chunk_take_in(void* data, md_buf_chunk_t* chunk) {

	md_log("Done decoding file.", chunk->metadata->fname);

	return;
}

static void md_last_chunk_take_out(void* data, md_buf_chunk_t* chunk) {

	md_log("Done playing file %s.", chunk->metadata->fname);

	return;
}

static void md_melodeer_stopped(void* data) {

	md_log("Melodeer stopped.");

	return;
}

static void md_melodeer_playing(void* data) {

	md_log("Melodeer playing");

	return;
}

static void md_melodeer_paused(void* data) {

	md_log("Melodeer paused");

	return;
}

static void md_buffer_underrun(void* data, bool critical) {

	md_log("Buffer underrun%s", critical ? " (!)" : "");

	return;
}

static void md_will_load_chunk(void* data, md_buf_chunk_t* buf_chunk) {

	return;
}

static void md_first_chunk_take_in(void* data, md_buf_chunk_t* chunk) {

	return;
}

static md_core_ops_t md_core_ops = {
	.will_load_chunk = md_will_load_chunk,
	.loaded_metadata = md_loaded_metadata,
	.first_chunk_take_in = md_first_chunk_take_in,
	.last_chunk_take_in = md_last_chunk_take_in,
	.last_chunk_take_out = md_last_chunk_take_out,
	.stopped = md_melodeer_stopped,
	.playing = md_melodeer_playing,
	.paused = md_melodeer_paused,
	.buffer_underrun = md_buffer_underrun,
	.data = NULL
};
*/

static md_playlist_ops_t md_playlist_ops = {
	.started_playing = NULL,
	.paused_playing = NULL,
	.stopped_playing = NULL,
	.removed_from_list = NULL,
	.added_to_list = NULL
};

int main (int argc, const char* argv[]) {
	int i;

	if (argc <= 1) {
		md_error("Please specify files to play.");
		md_deinit(true);

		return -EINVAL;
	}

	signal(SIGINT, sigint_handler);

	md_log("Got playlist of size: %d", argc - 1);

	md_playlist_init(&md_playlist_ops);

	for (i = 1; i < argc; i++)
		md_playlist_append(argv[i]);

	md_playlist_play(0);

	md_log("Deinitializing...");

	md_playlist_deinit();

	return 0;
}
