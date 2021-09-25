#include "mdbuf.h"

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include "mdlog.h"
#include "mdsettings.h"
#include "mdcoreops.h"
#include "mdgarbage.h"
#include "mdevq.h"
#include "mddecoder.h"
#include "mdpredict.h"

#define get_bufll_pack(buf_pack)					\
	get_pack_data(buf_pack, struct md_bufll_pack_t)

#define get_buf_num() ({						\
	md_buf_head ? md_buf_last->order - md_buf_head->order + 1 : 0;	\
})

static struct md_buf_ll {
	md_buf_chunk_t* chunk;
	struct md_buf_ll* next;
	int order;
} *md_buf_head, *md_buf_last;

struct md_bufll_pack_t {
	struct md_buf_ll* head;
	struct md_buf_ll* curr;
};

static struct md_bufll_t {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool run;
	bool error;
} md_bufll;

int md_buf_init(void) {

	if (md_unlikely(md_garbage_init(get_settings()->buf_num))) {
		md_error("Could not initialize garbage collector.");
		return -ENOMEM;
	}

	pthread_mutex_init(&md_bufll.mutex, NULL);
	pthread_cond_init(&md_bufll.cond, NULL);

	md_buf_head = NULL;
	md_buf_last = NULL;
	md_bufll.run = true;
	md_bufll.error = false;

	md_log("Buffer initialized.");

	return 0;
}

static bool md_buf_add_cond(void) {

	return get_buf_num() >= get_settings()->max_buf_num;
}

void md_buf_signal_error(void) {

	pthread_mutex_lock(&md_bufll.mutex);
	md_bufll.run = false;
	md_bufll.error = true;
	pthread_cond_signal(&md_bufll.cond);
	pthread_mutex_unlock(&md_bufll.mutex);

	return;
}

int md_buf_add(md_buf_chunk_t* buf_chunk) {
	struct md_buf_ll* new_buf;
	int ret;

	ret = 0;

	new_buf = malloc(sizeof(*new_buf));
	if (!new_buf) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	new_buf->chunk = buf_chunk;
	new_buf->next = NULL;

	pthread_mutex_lock(&md_bufll.mutex);
	while (md_bufll.run && md_buf_add_cond())
		pthread_cond_wait(&md_bufll.cond, &md_bufll.mutex);

	if (md_unlikely(!md_bufll.run)) {
		ret = md_bufll.error ? MD_BUF_ERROR : MD_BUF_EXIT;
		md_bufll.error = false;
		pthread_mutex_unlock(&md_bufll.mutex);
		return ret;
	}

	if (md_unlikely(!md_buf_head)) {
		new_buf->order = 0;
		md_buf_head = new_buf;
		md_buf_last = md_buf_head;
	}
	else {
		new_buf->order = md_buf_last->order + 1;
		md_buf_last->next = new_buf;
		md_buf_last = new_buf;
	}

	pthread_cond_signal(&md_bufll.cond);
	pthread_mutex_unlock(&md_bufll.mutex);

	return ret;
}

static void md_buf_free(struct md_buf_ll* curr) {
	struct md_buf_ll* temp;

	temp = curr;
	free(temp);

	return;
}

md_buf_chunk_t* md_buf_get_head(void) {
	md_buf_chunk_t* ret;

	/* Nothing in decoder or buffer code should ever
	 * remove head when it appears: it is up to driver
	 * to make sure -it- does not remove head when
	 * dealing with this pointer. */

	pthread_mutex_lock(&md_bufll.mutex);
	while (!md_buf_head && md_bufll.run) {
		pthread_cond_wait(&md_bufll.cond, &md_bufll.mutex);
	}
	ret = md_buf_head ? md_buf_head->chunk : NULL;
	pthread_mutex_unlock(&md_bufll.mutex);

	return ret;
}

int md_buf_get(md_buf_chunk_t** buf_chunk) {
	struct md_buf_ll* temp;

	pthread_mutex_lock(&md_bufll.mutex);
	while (!md_buf_head)
		pthread_cond_wait(&md_bufll.cond, &md_bufll.mutex);

	*buf_chunk = md_buf_head->chunk;
	temp = md_buf_head;
	md_buf_head = md_buf_head->next;
	md_buf_free(temp);

	pthread_cond_signal(&md_bufll.cond);
	pthread_mutex_unlock(&md_bufll.mutex);

	return 0;
}

md_buf_chunk_t* md_bufll_first(md_buf_pack_t* buf_pack) {
	md_buf_chunk_t* curr_chunk;

	if (md_unlikely(!get_bufll_pack(buf_pack)->head))
		return NULL;

	get_bufll_pack(buf_pack)->curr = get_bufll_pack(buf_pack)->head;
	curr_chunk = get_bufll_pack(buf_pack)->curr->chunk;

	md_evq_run_queue(curr_chunk->evq, MD_EVENT_RUN_ON_TAKE_IN);
	md_will_load_chunk_exec(curr_chunk);

	return curr_chunk;
}

md_buf_chunk_t* md_bufll_next(md_buf_pack_t* buf_pack) {
	struct md_buf_ll* curr;
	md_buf_chunk_t* curr_chunk;

	curr = get_bufll_pack(buf_pack)->curr;
	if (md_unlikely(!curr))
		return NULL;

	get_bufll_pack(buf_pack)->curr = (curr = curr->next);

	if (md_likely(curr)) {
		curr_chunk = curr->chunk;
		md_evq_run_queue(curr_chunk->evq, MD_EVENT_RUN_ON_TAKE_IN);
		md_will_load_chunk_exec(curr_chunk);

		return curr_chunk;
	}

	return NULL;
}

static void md_bufll_free(struct md_buf_ll* curr) {
	struct md_buf_ll* temp;

	temp = curr;
	free(temp);

	return;
}

void md_buf_clean_pack(md_buf_pack_t* buf_pack) {
	struct md_buf_ll* curr;
	struct md_buf_ll* next;

	curr = get_bufll_pack(buf_pack)->head;

	while (curr) {
		md_add_to_garbage(curr->chunk);
		next = curr->next;
		md_bufll_free(curr);
		curr = next;
	}

	free(get_bufll_pack(buf_pack));
	free(buf_pack);

	return;
}

bool md_buf_is_empty(void) {
	bool is_empty;

	pthread_mutex_lock(&md_bufll.mutex);
	is_empty = md_buf_head == NULL;
	pthread_mutex_unlock(&md_bufll.mutex);

	return is_empty;
}

static bool md_buf_get_pack_cond(int count, md_pack_mode_t mode) {

	if (md_likely(mode == MD_PACK_EXACT)) {

		if (get_buf_num() >= count)
			return false;

		else if (md_unlikely(md_buf_last
		      && md_is_decoder_done(md_buf_last->chunk)
		      && md_no_more_decoders()))
			return false;
		else
			return true;
	}

	return !get_buf_num();
}

#define md_will_stop_on(bufll) \
	(md_is_decoder_done((bufll)->chunk) && md_no_more_decoders())

int md_buf_get_pack(md_buf_pack_t** buf_pack, int* count,
		    md_pack_mode_t pack_mode) {
	struct md_buf_ll* curr;
	struct md_bufll_pack_t* md_bufll_pack;
	int i, ret;

	ret = 0;

	*buf_pack = malloc(sizeof(**buf_pack));
	if (!(*buf_pack)) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	md_bufll_pack = malloc(sizeof(*md_bufll_pack));
	if (!(md_bufll_pack)) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	(*buf_pack)->first = md_bufll_first;
	(*buf_pack)->next = md_bufll_next;
	(*buf_pack)->data = md_bufll_pack;

	pthread_mutex_lock(&md_bufll.mutex);
	while (md_bufll.run && md_buf_get_pack_cond(*count, pack_mode))
		pthread_cond_wait(&md_bufll.cond, &md_bufll.mutex);

	if (md_unlikely(!md_bufll.run)) {
		ret = md_bufll.error ? MD_BUF_ERROR : MD_BUF_EXIT;
		md_bufll.error = false;
		pthread_mutex_unlock(&md_bufll.mutex);
		free(*buf_pack);
		free(md_bufll_pack);

		return ret;
	}

	get_bufll_pack(*buf_pack)->head = md_buf_head;
	get_bufll_pack(*buf_pack)->curr = md_buf_head;
	curr = md_buf_head;

	for (i = 0; i < *count; i++) {

		if (md_unlikely(!curr)) {
			*count = i;
			if (md_unlikely(*count != i))
				ret = MD_PACK_EXACT_NO_MORE;
			break;
		}
		else if (md_unlikely(md_will_stop_on(curr))) {
			curr = curr->next;
			*count = i + 1;
			ret = MD_BUF_NO_DECODERS;

			break;
		}
		else if (md_likely(i < *count - 1))
			curr = curr->next;
	}

	if (md_likely(curr)) {
		md_buf_head = curr->next;
		if (!md_buf_head)
			md_buf_last = NULL;
		curr->next = NULL;
	}
	else {
		md_buf_head = NULL;
		md_buf_last = NULL;
	}

	pthread_cond_signal(&md_bufll.cond);
	pthread_mutex_unlock(&md_bufll.mutex);

	return ret;
}

void md_buf_flush(void) {
	struct md_buf_ll* curr;
	struct md_buf_ll* next;

	pthread_mutex_lock(&md_bufll.mutex);

	curr = md_buf_head;

	while (curr) {
		next = curr->next;
		free(curr->chunk);
		md_buf_free(curr);
		curr = next;
	}

	md_buf_head = NULL;
	md_buf_last = NULL;
	md_bufll.run = false;

	pthread_cond_signal(&md_bufll.cond);
	pthread_mutex_unlock(&md_bufll.mutex);

	return;
}

void md_buf_resume(void) {

	pthread_mutex_lock(&md_bufll.mutex);
	md_bufll.run = true;
	pthread_mutex_unlock(&md_bufll.mutex);

	return;
}

int md_buf_deinit(void) {

	md_buf_flush();

	pthread_mutex_destroy(&md_bufll.mutex);
	pthread_cond_destroy(&md_bufll.cond);

	return 0;
}

