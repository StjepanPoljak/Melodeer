#include "mdbuf.h"

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "mdlog.h"

#define get_bufll_pack(buf_pack) \
	get_pack_data(buf_pack, struct md_bufll_pack_t)

static struct md_buf_ll {
	md_buf_chunk_t* chunk;
	struct md_buf_ll* next;
} *md_buf_head, *md_buf_last;

struct md_bufll_pack_t {
	const struct md_buf_ll* head;
	const struct md_buf_ll* curr;
};

static const md_settings_t* mdsettings;
static pthread_mutex_t bufll_mutex;

int md_buf_init(const md_settings_t* settings) {

	md_buf_head = NULL;
	md_buf_last = NULL;
	mdsettings = settings;
	pthread_mutex_init(&bufll_mutex, NULL);

	return 0;
}

int md_buf_add(md_buf_chunk_t* buf_chunk) {
	struct md_buf_ll* new_buf;

	new_buf = malloc(sizeof(*new_buf));
	if (!new_buf) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	new_buf->chunk = buf_chunk;
	new_buf->next = NULL;

	pthread_mutex_lock(&bufll_mutex);
	if (!md_buf_head) {
		md_buf_head = new_buf;
		md_buf_last = new_buf;
	}
	else {
		md_buf_last->next = new_buf;
		md_buf_last = new_buf;
	}
	pthread_mutex_unlock(&bufll_mutex);

	return 0;
}

static void md_buf_free(struct md_buf_ll* curr) {
	struct md_buf_ll* temp;

	temp = curr;
	free(temp);

	return;
}

int md_buf_get(const md_buf_chunk_t** buf_chunk) {
	struct md_buf_ll* temp;

	pthread_mutex_lock(&bufll_mutex);
	if (md_buf_head) {
		*buf_chunk = md_buf_head->chunk;
		temp = md_buf_head;
		md_buf_head = md_buf_head->next;
		md_buf_free(temp);
	}
	pthread_mutex_unlock(&bufll_mutex);

	return 0;
}

const md_buf_chunk_t* md_bufll_first(const md_buf_pack_t* buf_pack) {

	get_bufll_pack(buf_pack)->curr = get_bufll_pack(buf_pack)->head;

	return get_bufll_pack(buf_pack)->curr->chunk;
}

const md_buf_chunk_t* md_bufll_next(const md_buf_pack_t* buf_pack) {
	const struct md_buf_ll* curr;

	curr = get_bufll_pack(buf_pack)->curr;
	if (!curr)
		return NULL;

	curr = curr->next;

	return curr ? curr->chunk : NULL;
}

int md_buf_get_pack(md_buf_pack_t** buf_pack, int* count) {
	struct md_buf_ll* curr;
	int i;

	*buf_pack = malloc(sizeof(**buf_pack));
	if (!(*buf_pack)) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	(*buf_pack)->first = md_bufll_first;
	(*buf_pack)->next = md_bufll_next;

	pthread_mutex_lock(&bufll_mutex);
	get_bufll_pack(*buf_pack)->head = md_buf_head;
	get_bufll_pack(*buf_pack)->curr = md_buf_head;
	curr = md_buf_head;

	for (i = 0; i < *count; i++) {

		if (curr)
			curr = curr->next;
		else {
			*count = i + 1;
			break;
		}
	}

	if (curr) {
		curr->next = NULL;
		md_buf_head = curr->next;
	}
	else {
		md_buf_head = NULL;
		md_buf_last = NULL;
	}
	pthread_mutex_unlock(&bufll_mutex);

	return 0;
}

int md_buf_deinit(void) {
	struct md_buf_ll* curr;
	struct md_buf_ll* next;

	pthread_mutex_lock(&bufll_mutex);
	curr = md_buf_head;

	while (curr) {
		next = curr->next;
		free(curr->chunk);
		md_buf_free(curr);
		curr = next;
	}
	pthread_mutex_unlock(&bufll_mutex);

	pthread_mutex_destroy(&bufll_mutex);

	return 0;
}


