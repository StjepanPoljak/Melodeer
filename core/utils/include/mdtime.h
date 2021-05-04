#ifndef MD_TIME_H
#define MD_TIME_H

#include "mdmetadata.h"

unsigned long md_buf_len_usec(md_metadata_t*, int);
double md_buf_len_sec(md_metadata_t* metadata, int buf_size);

#endif
