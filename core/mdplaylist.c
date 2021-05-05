#include "mdplaylist.h"

#include <string.h>
#include <pthread.h>

#include "mdcore.h"
#include "mdlog.h"
#include "mdtime.h"
#include "mdpredict.h"

typedef enum {
	MD_PL_SWITCH_PERCENT,
	MD_PL_SWITCH_USECONDS,
	MD_PL_SWITCH_BUFF_CHUNKS
} md_pl_switch_t;

typedef struct {
	md_pl_switch_t pl_switch;
	unsigned long pl_switch_value;
} md_pl_settings_t;

static struct md_playlist_t {
	int size;
	const char** list;
	bool running;
	int curr;
	md_pl_settings_t settings;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	md_core_ops_t md_core_ops;
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

static void md_pl_stopped_cb(void* data) {

	md_playlist_done();

	return;
}

static void md_playlist_setup_start_event(void* data,
					  md_buf_chunk_t* buf_chunk,
					  int* decoder_start_event) {
	int buf_chunks;

	switch (mdplaylist.settings.pl_switch) {
	case MD_PL_SWITCH_PERCENT:
		*decoder_start_event = (buf_chunk->metadata->total_buf_chunks
				     *  mdplaylist.settings.pl_switch_value)
				     /  100;
		break;

	case MD_PL_SWITCH_USECONDS:
		buf_chunks = md_usec_to_buf_chunks(
				buf_chunk->metadata, buf_chunk->size,
				mdplaylist.settings.pl_switch_value);
		*decoder_start_event = (buf_chunk->metadata->total_buf_chunks
				     >  buf_chunks)
				     * (buf_chunk->metadata->total_buf_chunks
				     -  buf_chunks);
		break;

	case MD_PL_SWITCH_BUFF_CHUNKS:

		*decoder_start_event = (buf_chunk->metadata->total_buf_chunks
				     >  mdplaylist.settings.pl_switch_value)
				     * (buf_chunk->metadata->total_buf_chunks
				     -  mdplaylist.settings.pl_switch_value);
		break;

	default:
		break;

	}

	md_log("Will start new at %d (%d)", *decoder_start_event,
	       buf_chunk->metadata->total_buf_chunks);

}

static void md_pl_will_load_chunk_cb(void* data, md_buf_chunk_t* buf_chunk) {
	static int decoder_start_event = 0;
	
	if (md_unlikely(mdplaylist.curr >= mdplaylist.size))
		return;

	if (md_unlikely(buf_chunk->order == 0)) {

		md_log("Got file time: %.4fs (%s)",
		       md_buf_len_sec(buf_chunk->metadata, buf_chunk->size)
		     * buf_chunk->metadata->total_buf_chunks,
		       buf_chunk->metadata->fname);

		md_playlist_setup_start_event(data, buf_chunk,
					      &decoder_start_event);
	}

	if (md_unlikely(buf_chunk->order == decoder_start_event))
		md_play_async(mdplaylist.list[mdplaylist.curr++], NULL);

	decoder_start_event =  decoder_start_event
			    * (buf_chunk->order
			   !=  buf_chunk->metadata->total_buf_chunks - 1);

	mdplaylist.will_load_chunk_usr_cb(mdplaylist.md_core_ops.data,
					  buf_chunk);

	return;
}


static void md_core_wait(void) {

	pthread_mutex_lock(&mdplaylist.mutex);
	while (mdplaylist.running)
		pthread_cond_wait(&mdplaylist.cond, &mdplaylist.mutex);
	pthread_mutex_unlock(&mdplaylist.mutex);

	return;
}

void md_playlist_init(md_core_ops_t* md_pl_ops) {

	md_init();

	memcpy(&mdplaylist.md_core_ops, md_pl_ops,
	       sizeof(mdplaylist.md_core_ops));

	mdplaylist.md_core_ops.will_load_chunk = md_pl_will_load_chunk_cb;
	mdplaylist.md_core_ops.stopped = md_pl_stopped_cb;

	mdplaylist.will_load_chunk_usr_cb = md_pl_ops->will_load_chunk;
	mdplaylist.stopped_usr_cb = md_pl_ops->stopped;

	md_set_core_ops(&mdplaylist.md_core_ops);

	mdplaylist.running = true;

	//mdplaylist.settings.pl_switch = MD_PL_SWITCH_PERCENT;
	//mdplaylist.settings.pl_switch_value = 70;

	mdplaylist.settings.pl_switch = MD_PL_SWITCH_USECONDS;
	mdplaylist.settings.pl_switch_value = 60000000;

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
