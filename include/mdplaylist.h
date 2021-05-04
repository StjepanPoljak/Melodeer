#ifndef MD_PLAYLIST_H
#define MD_PLAYLIST_H

#include "mdcoreops.h"

void md_playlist_init(md_core_ops_t*);
int md_playlist_play(int, const char*[]);
void md_playlist_deinit(void);

#endif
