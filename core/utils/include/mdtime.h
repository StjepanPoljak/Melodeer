#ifndef MD_TIME_H
#define MD_TIME_H

#include "mdmetadata.h"

unsigned long md_buf_len_usec(md_metadata_t*, int);
int md_usec_to_buf_chunks(md_metadata_t*, int, unsigned long);
double md_buf_len_sec(md_metadata_t*, int buf_size);

#endif
