#ifndef MD_PLAYLIST_H
#define MD_PLAYLIST_H

#include <stdbool.h>

#define PL_EXEC(fn, ...) do {						\
	if (md_likely(md_playlist_ops && md_playlist_ops->fn)) {	\
		md_playlist_ops->fn(__VA_ARGS__);			\
	}								\
} while (0)

typedef struct {
	void (*started_playing)(int);
	void (*paused_playing)(void);
	void (*stopped_playing)(void);
	void (*removed_from_list)(int);
	void (*added_to_list)(int);
} md_playlist_ops_t;

int md_playlist_init(md_playlist_ops_t*);
int md_playlist_append(const char* fpath);
int md_playlist_insert(const char* fpath, int after);
int md_playlist_remove(int el);
int md_playlist_play(int el);
void md_playlist_deinit(void);
bool md_no_more_decoders(void);

#endif
