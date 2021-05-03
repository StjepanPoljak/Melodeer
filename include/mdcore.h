#ifndef MD_CORE_H
#define MD_CORE_H

#include "mddriver.h"

int md_init(void);
void md_stop(void);
void md_resume(void);
int md_play_async(const char*, const char*);
char* md_get_logo(void);
void md_deinit(bool);

#define md_pause() do {		\
	md_driver_pause();	\
} while (0)

#define md_unpause() do {	\
	md_driver_unpause();	\
} while (0)

#endif
