#include "mdplaylist.h"

#include <stdlib.h>
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

struct md_pl_ll_t {
	int index;
	char* fname;
	struct md_pl_ll_t* next;
	struct md_pl_ll_t* prev;
};

typedef struct md_pl_ll_t md_pl_ll_t;

static struct md_playlist_t {
	int size;
	md_pl_ll_t* curr;
	md_pl_ll_t* head;
	md_pl_ll_t* last;

	bool running;
	md_pl_settings_t settings;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	md_core_ops_t md_core_ops;
	void(*will_load_chunk_usr_cb)(void*, md_buf_chunk_t*);
	void(*stopped_usr_cb)(void*);
	void(*last_chunk_take_in_usr_cb)(void*, md_buf_chunk_t*);
	void(*started_playing_file)(int, const char*, void*, md_buf_chunk_t*);
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

static void md_pl_started_playing_file_default_cb(int index,
						  const char* fname,
						  void* data,
						  md_buf_chunk_t* buf_chunk) {
	(void)data;

	md_log("Got file (%d) time: %.4fs (%s)", index,
	       md_buf_len_sec(buf_chunk->metadata, buf_chunk->size)
	     * buf_chunk->metadata->total_buf_chunks,
	       fname);

	return;
}

static void md_pl_will_load_chunk_cb(void* data, md_buf_chunk_t* buf_chunk) {
	static int decoder_start_event = 0;
	
	if (md_unlikely(!mdplaylist.curr))
		return;

	if (md_unlikely(buf_chunk->order == 0)) {

		if (md_likely(mdplaylist.curr->next))
			md_playlist_setup_start_event(data, buf_chunk,
						      &decoder_start_event);

		mdplaylist.started_playing_file(mdplaylist.curr->index,
						mdplaylist.curr->fname,
						data, buf_chunk);
	}

	if (md_unlikely(buf_chunk->order == decoder_start_event)) {
		if (md_likely(mdplaylist.curr->next))
			md_play_async(mdplaylist.curr->next->fname, NULL);
	}

	decoder_start_event =  decoder_start_event
			    * (buf_chunk->order
			   !=  buf_chunk->metadata->total_buf_chunks - 1);

	mdplaylist.will_load_chunk_usr_cb(mdplaylist.md_core_ops.data,
					  buf_chunk);

	return;
}

static void md_pl_last_chunk_take_in_cb(void* data, md_buf_chunk_t* chunk) {
	(void)data, (void)chunk;

	mdplaylist.curr = mdplaylist.curr->next;

	return;
}

static void md_core_wait(void) {

	pthread_mutex_lock(&mdplaylist.mutex);
	while (mdplaylist.running)
		pthread_cond_wait(&mdplaylist.cond, &mdplaylist.mutex);
	pthread_mutex_unlock(&mdplaylist.mutex);

	return;
}

int md_playlist_append(const char* fname) {
	md_pl_ll_t* new;

	new = malloc(sizeof(*new));
	if (!new) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	new->fname = strdup(fname);

	if (md_unlikely(!mdplaylist.last && !mdplaylist.head)) {
		new->next = NULL;
		new->prev = NULL;
		new->index = 0;
		mdplaylist.last = new;
		mdplaylist.head = new;
	}
	else {
		new->next = NULL;
		new->prev = mdplaylist.last;
		new->index = mdplaylist.last->index + 1;
		mdplaylist.last->next = new;
		mdplaylist.last = new;
	}

	mdplaylist.size++;

	return 0;
}

void md_playlist_init(md_core_ops_t* md_pl_ops,
		      void(*started_playing_file)(int, const char*,
						  void*, md_buf_chunk_t*)) {

	memcpy(&mdplaylist.md_core_ops, md_pl_ops,
	       sizeof(mdplaylist.md_core_ops));

	mdplaylist.md_core_ops.will_load_chunk = md_pl_will_load_chunk_cb;
	mdplaylist.md_core_ops.stopped = md_pl_stopped_cb;
	mdplaylist.md_core_ops.last_chunk_take_in
					= md_pl_last_chunk_take_in_cb;

	mdplaylist.will_load_chunk_usr_cb = md_pl_ops->will_load_chunk;
	mdplaylist.stopped_usr_cb = md_pl_ops->stopped;
	mdplaylist.last_chunk_take_in_usr_cb = md_pl_ops->last_chunk_take_in;

	mdplaylist.started_playing_file
				= started_playing_file
				? started_playing_file
				: md_pl_started_playing_file_default_cb;

	mdplaylist.curr = NULL;
	mdplaylist.head = NULL;
	mdplaylist.last = NULL;
	mdplaylist.size = 0;

	mdplaylist.running = true;

	//mdplaylist.settings.pl_switch = MD_PL_SWITCH_PERCENT;
	//mdplaylist.settings.pl_switch_value = 70;

	mdplaylist.settings.pl_switch = MD_PL_SWITCH_USECONDS;
	mdplaylist.settings.pl_switch_value = 60000000;

	pthread_mutex_init(&mdplaylist.mutex, NULL);
	pthread_cond_init(&mdplaylist.cond, NULL);

	md_init(&mdplaylist.md_core_ops);

	return;
}

int md_playlist_play(void) {
	int ret;

	mdplaylist.curr = mdplaylist.head;
	ret = md_play_async(mdplaylist.curr->fname, NULL);
	if (ret) {
		md_error("Could not start playlist.");
		return ret;
	}

	md_core_wait();

	return 0;
}

void md_playlist_deinit(void) {

	md_deinit(false);

	pthread_mutex_destroy(&mdplaylist.mutex);
	pthread_cond_destroy(&mdplaylist.cond);

	return;
}
