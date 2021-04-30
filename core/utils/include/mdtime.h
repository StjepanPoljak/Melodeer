#ifndef MD_TIME_H
#define MD_TIME_H

#include "mdmetadata.h"

unsigned long md_buff_bytes_per_sec(md_metadata_t*, int);
double md_buff_usec(md_metadata_t*, int);
double md_buff_msec(md_metadata_t*, int);
double md_buff_sec(md_metadata_t*, int);

#endif
