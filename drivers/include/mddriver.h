#ifndef MD_DRIVER_H
#define MD_DRIVER_H

#include <stdbool.h>
#include <errno.h>

#include "mdbufchunk.h"
#include "mdsym.h"

typedef enum {
	MD_DRIVER_PLAYING,
	MD_DRIVER_STOPPED,
	MD_DRIVER_PAUSED
} md_driver_state_t;

typedef enum {
	MD_DRIVER_STATE_SET,
	MD_DRIVER_STATE_WILL_BE_SET,
	MD_DRIVER_STATE_SAME,
	MD_DRIVER_STATE_NOT_SET
} md_driver_state_ret_t;

typedef struct {
	int(*load_symbols)(void);
	int(*init)(void);
	md_driver_state_ret_t(*pause)(void);
	md_driver_state_ret_t(*unpause)(void);
	md_driver_state_ret_t(*stop)(void);
	md_driver_state_ret_t(*resume)(void);
	int(*get_state)(md_driver_state_t*);
	int(*deinit)(void);
} md_driver_ops;

typedef struct {
	const char* name;
	const char* lib;
	void* handle;
	md_driver_ops ops;
} md_driver_t;

struct md_driver_ll {
	md_driver_t* driver;
	void* handle;
	struct md_driver_ll* next;
};

typedef struct md_driver_ll md_driver_ll;

md_driver_ll* md_driver_ll_add(md_driver_t*);
md_driver_t* md_driver_ll_find(const char*);
int md_driver_ll_deinit(void);

#define register_driver(_name, _driver)				\
	__attribute__((constructor(105)))			\
	void driver_register_##_name(void) {			\
		if (!md_driver_ll_add(&(_driver)))		\
			md_error("Could not add " #_name ".");	\
		else						\
			md_log("Loaded driver " #_name ".");	\
	}

int md_driver_init(void);
void md_driver_buffer_underrun_event(bool);
void md_driver_signal_state(md_driver_state_t);
md_driver_state_ret_t md_driver_pause(void);
md_driver_state_ret_t md_driver_unpause(void);
md_driver_state_ret_t md_driver_stop(void);
md_driver_state_ret_t md_driver_resume(void);
void md_driver_error_event(void);
int md_driver_deinit(void);

DECLARE_SYM_FUNCTIONS(driver);

#endif
