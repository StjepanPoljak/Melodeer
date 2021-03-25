#ifndef MD_SETTINGS_H
#define MD_SETTINGS_H

#include "mddriver.h"

typedef struct {
	int buf_size;
	int buf_num;
	int min_buf_num;
	md_driver_t* driver;
} md_settings_t;

void load_settings(void);
md_settings_t* get_settings(void);

#endif
