#ifndef MD_PLAYLIST_H
#define MD_PLAYLIST_H

#include "mdcoreops.h"
#include "mdbufchunk.h"

void md_playlist_init(md_core_ops_t*,
		      void(*started_playing_file)(int, const char*,
						  void*, md_buf_chunk_t*));
int md_playlist_play(void);
int md_playlist_append(const char*);
void md_playlist_deinit(void);

#endif
