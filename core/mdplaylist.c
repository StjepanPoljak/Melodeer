#include "mdplaylist.h"

#include <pthread.h>
#include <string.h>

#include "mddecoder.h"
#include "mdpredict.h"
#include "mdcoreops.h"

#define PL_STARTING_SIZE 0

#define pl_lock() do { 				\
	pthread_mutex_lock(&pl_mutex);		\
} while (0)

#define pl_unlock() do {			\
	pthread_mutex_unlock(&pl_mutex);	\
} while (0)

#define PL_ASSERT(fn, ...) do {			\
	int ret;				\
	if ((ret = fn(__VA_ARGS__))) {		\
		pl_unlock();			\
		return ret;			\
	}					\
} while (0)

int pl_size;
int pl_last;
int pl_active;
pthread_mutex_t pl_mutex;
md_decoder_data_t** pl_dec_array;
md_playlist_ops_t* md_playlist_ops;

static md_core_ops_t pl_core_ops = {
	.will_load_chunk = NULL,
	.loaded_metadata = NULL,
	.first_chunk_take_in = NULL,
	.last_chunk_take_in = NULL,
	.last_chunk_take_out = NULL,
	.stopped = NULL,
	.playing = NULL,
	.paused = NULL,
	.buffer_underrun = NULL,
	.data = NULL
};

int md_playlist_init(md_playlist_ops_t* ops) {

	pl_size = PL_STARTING_SIZE;
	pl_last = -1;
	pl_active = -1;
	md_playlist_ops = ops;
	md_set_core_ops(&pl_core_ops);

	pthread_mutex_init(&pl_mutex, NULL);

	pl_dec_array = PL_STARTING_SIZE > 0
		     ? malloc(sizeof(*pl_dec_array) * PL_STARTING_SIZE)
		     : NULL;

	return 0;
}

static inline int __md_playlist_adjust_size(void) {
	md_decoder_data_t** temp;

	if (md_unlikely(!pl_dec_array)) {
		pl_size = 1;
		pl_dec_array = malloc(sizeof(*pl_dec_array));

		if (md_unlikely(!pl_dec_array))
			return -ENOMEM;

		return 0;
	}

	if (md_unlikely(pl_size <= pl_last + 1)) {
		pl_size *= 2;
		memcpy(temp, pl_dec_array, pl_last + 1);
		pl_dec_array = realloc(pl_dec_array,
				       sizeof(*pl_dec_array) * pl_size);

		if (md_unlikely(!pl_dec_array))
			return -ENOMEM;

		memcpy(pl_dec_array, temp, pl_last + 1);

		return 0;
	}

	return 0;
}

static inline int __md_playlist_alloc(int el) {

	if (md_unlikely(el >= pl_size))
		return -EINVAL;

	pl_dec_array[el] = malloc(sizeof(*(pl_dec_array[el])));
	if (md_unlikely(!pl_dec_array[el]))
		return -ENOMEM;

	return 0;
}

int md_playlist_append(const char* fpath) {

	pl_lock();
	PL_ASSERT(__md_playlist_adjust_size);
	PL_ASSERT(__md_playlist_alloc, ++pl_last);
	md_decoder_init(pl_dec_array[pl_last], fpath, NULL);
	PL_EXEC(added_to_list, pl_last);
	pl_unlock();

	return 0;
}

int md_playlist_insert_at(const char* fpath, int el) {
	unsigned int i;

	pl_lock();
	PL_ASSERT(__md_playlist_adjust_size);
	pl_last++;

	for (i = pl_last; i > el; i--)
		pl_dec_array[i] = pl_dec_array[i - 1];

	PL_ASSERT(__md_playlist_alloc, el);
	md_decoder_init(pl_dec_array[el], fpath, NULL);
	pl_unlock();

	return 0;
}

int md_playlist_remove(int el) {

	pl_lock();

	if (md_unlikely(pl_active == el)) {
		pl_unlock();
		return -EINVAL;
	}

	PL_EXEC(removed_from_list, el);

	pl_unlock();

	return 0;
}

int md_playlist_play(int el) {

	return 0;
}

void md_playlist_deinit(void) {

	pl_lock();

	pl_unlock();

	pthread_mutex_destroy(&pl_mutex);

	return;
}

void for_each_in_pl(bool (*fn)(const char*, md_metadata_t*)) {
	unsigned int i;

	pl_lock();

	for (i = 0; i <= pl_last; i++) {
		if (md_unlikely(!fn(pl_dec_array[i]->fpath,
				    pl_dec_array[i]->metadata)))
		break;
	}

	pl_unlock();

	return;
}

bool md_no_more_decoders(void) {

	return false;
}
