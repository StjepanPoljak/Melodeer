#ifndef MD_CORE_H
#define MD_CORE_H

#include "mdmetadata.h"

int md_init(void);
int md_set_metadata(md_metadata_t*);
char* md_get_logo(void);
void md_deinit(void);

#endif
