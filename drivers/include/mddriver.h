#ifndef MD_DRIVER_H
#define MD_DRIVER_H

#include <errno.h>

#include "mdmetadata.h"
#include "mdbufchunk.h"
#include "mdsym.h"

typedef enum {
	MD_DRIVER_PLAYING,
	MD_DRIVER_WAITING,
	MD_DRIVER_STOPPED,
	MD_DRIVER_PAUSED,
	MD_DRIVER_ERROR
} md_driver_state_t;

typedef struct {
	int(*load_symbols)(void);
	int(*init)(void);
	int(*set_metadata)(md_metadata_t*);
	int(*play)(void);
	int(*stop)(void);
	int(*pause)(void);
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

int md_driver_exec_events(md_buf_chunk_t*);

DECLARE_SYM_FUNCTIONS(driver);
/*int md_driver_try_load(md_driver_t*);
void md_driver_unload(md_driver_t*);
*/
#endif
