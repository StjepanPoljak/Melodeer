#ifndef MD_SETTINGS_H
#define MD_SETTINGS_H

#include <stdbool.h>

#include "mddriver.h"

typedef struct {
	int count;
	int secs;
} md_buf_underrun_settings_t;

typedef struct {
	int buf_size;
	int buf_num;
	int max_buf_num;
	md_buf_underrun_settings_t buf_underrun;
	md_driver_t* driver;
	bool verbose;
} md_settings_t;

int load_settings(void);
md_settings_t* get_settings(void);

#endif
