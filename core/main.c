#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>

#include "mdcore.h"
#include "mdcoreops.h"
#include "mdlog.h"

static bool volatile md_running = true;
static pthread_mutex_t mutex;
static pthread_cond_t cond;

static void md_core_done(void) {

	pthread_mutex_lock(&mutex);
	md_running = false;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);

	return;
}

static void md_core_wait(void) {

	pthread_mutex_lock(&mutex);
	while (md_running)
		pthread_cond_wait(&cond, &mutex);
	pthread_mutex_unlock(&mutex);

	return;
}

static void sigint_handler(int signal) {
	(void)signal;

	md_stop();

	return;
}

void md_loaded_metadata(void* data, md_buf_chunk_t* chunk,
			md_metadata_t* metadata) {

	md_log("Loaded metadata (ch=%d, bps=%d, rate=%d)",
	       metadata->channels, metadata->bps, metadata->sample_rate);

	return;
}

void md_last_chunk_take_in(void* data, md_buf_chunk_t* chunk) {

	md_log("Done decoding file.", chunk->metadata->fname);

	return;
}

void md_last_chunk_take_out(void* data, md_buf_chunk_t* chunk) {

	md_log("Done playing file %s.", chunk->metadata->fname);

	return;
}

void md_melodeer_stopped(void* data) {

	md_log("Melodeer stopped.");

	md_core_done();

	return;
}

void md_melodeer_playing(void* data) {

	md_log("Melodeer playing");

	return;
}

void md_melodeer_paused(void* data) {

	md_log("Melodeer paused");

	return;
}

void md_buffer_underrun(void* data, bool critical) {

	md_log("Buffer underrun%s", critical ? " (!)" : "");

	return;
}

static md_core_ops_t md_core_ops = {
	.will_load_chunk = NULL,
	.loaded_metadata = md_loaded_metadata,
	.last_chunk_take_in = md_last_chunk_take_in,
	.last_chunk_take_out = md_last_chunk_take_out,
	.stopped = md_melodeer_stopped,
	.playing = md_melodeer_playing,
	.paused = md_melodeer_paused,
	.buffer_underrun = md_buffer_underrun,
	.data = NULL
};

int main (int argc, char *argv[]) {
	int i;

	if (argc <= 1) {
		md_error("Please specify files to play.");
		md_deinit(true);

		return -EINVAL;
	}

	md_init();
	md_set_core_ops(&md_core_ops);

	signal(SIGINT, sigint_handler);

	for (i = 0; i < argc - 1; i++)
		md_play_async(argv[1], NULL);

	md_core_wait();

	md_log("Deinitializing...");

	md_deinit(false);

	return 0;
}
