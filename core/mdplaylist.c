#include "mdplaylist.h"

#include <string.h>
#include <pthread.h>

#include "mdcore.h"
#include "mdlog.h"
#include "mdtime.h"

static struct md_playlist_t {
	int size;
	const char** list;
	bool running;
	int curr;
	int percent;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	void(*will_load_chunk_usr_cb)(void*, md_buf_chunk_t*);
	void(*stopped_usr_cb)(void*);
} mdplaylist;

static void md_playlist_done(void) {

	pthread_mutex_lock(&mdplaylist.mutex);
	mdplaylist.running = false;
	pthread_cond_signal(&mdplaylist.cond);
	pthread_mutex_unlock(&mdplaylist.mutex);

	return;
}

static void md_playlist_stopped_cb(void* data) {

	md_playlist_done();

	return;
}

static void md_playlist_will_load_chunk_cb(void* data,
					   md_buf_chunk_t* buf_chunk) {
	static int decoder_start_event = 0;
	static unsigned long usec = 0;

	if (usec == 0) {
		usec = md_buf_len_usec(buf_chunk->metadata, buf_chunk->size);
		md_log("Got buf size in usec: %lu", usec);
	}

	if (mdplaylist.curr >= mdplaylist.size)
		return;

	if (buf_chunk->order == 0) {

		md_log("Got file time: %.4fs (%s)",
		       md_buf_len_sec(buf_chunk->metadata, buf_chunk->size)
		     * buf_chunk->metadata->total_buf_chunks,
		       buf_chunk->metadata->fname);

		decoder_start_event = (buf_chunk->metadata->total_buf_chunks
				     * mdplaylist.percent) / 100;
		md_log("Will start new at %d (%d)", decoder_start_event,
		       buf_chunk->metadata->total_buf_chunks);

		return;
	}

	if (buf_chunk->order == decoder_start_event) {

		md_log("Starting next decoder (time is %lus)...",
		       (usec * buf_chunk->order) / 1000000);
		md_play_async(mdplaylist.list[mdplaylist.curr++], NULL);

		return;
	}

	if (buf_chunk->order == buf_chunk->metadata->total_buf_chunks - 1) {

		md_log("Resetting start event for decoder.");
		decoder_start_event = 0;
	}

	return;
}


static void md_core_wait(void) {

	pthread_mutex_lock(&mdplaylist.mutex);
	while (mdplaylist.running)
		pthread_cond_wait(&mdplaylist.cond, &mdplaylist.mutex);
	pthread_mutex_unlock(&mdplaylist.mutex);

	return;
}

static md_core_ops_t md_core_ops;

void md_playlist_init(md_core_ops_t* md_pl_ops) {

	md_init();

	memcpy(&md_core_ops, md_pl_ops, sizeof(md_core_ops));

	md_core_ops.will_load_chunk = md_playlist_will_load_chunk_cb;
	md_core_ops.stopped = md_playlist_stopped_cb;

	mdplaylist.will_load_chunk_usr_cb = md_pl_ops->will_load_chunk;
	mdplaylist.stopped_usr_cb = md_pl_ops->stopped;

	md_set_core_ops(&md_core_ops);

	mdplaylist.running = true;
	mdplaylist.percent = 70;

	pthread_mutex_init(&mdplaylist.mutex, NULL);
	pthread_cond_init(&mdplaylist.cond, NULL);

	return;
}

int md_playlist_play(int count, const char* playlist[]) {

	mdplaylist.size = count;
	mdplaylist.list = playlist;

	mdplaylist.curr = 0;
	md_play_async(mdplaylist.list[mdplaylist.curr++], NULL);

	md_core_wait();
}

void md_playlist_deinit(void) {

	md_deinit(false);

	pthread_mutex_destroy(&mdplaylist.mutex);
	pthread_cond_destroy(&mdplaylist.cond);

	return;
}
