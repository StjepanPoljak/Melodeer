#ifndef MD_GARBAGE_H
#define MD_GARBAGE_H

#include "mdbufchunk.h"

int md_garbage_init(int);
bool md_garbage_is_full(void);
bool md_garbage_del_first(void);
void md_add_to_garbage(md_buf_chunk_t*);
void md_garbage_clean(void);
void md_garbage_deinit(void);

#endif
