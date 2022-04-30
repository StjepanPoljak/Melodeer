#ifndef MD_DECODERLL_H
#define MD_DECODERLL_H

#include <stdlib.h>
#include <stdbool.h>

#include "mdll.h"
#include "mdsym.h"

struct md_decoder_data_t;

typedef struct {
	int(*load_symbols)(void);
	bool(*decodes_file)(const char*);
	int(*decode_file)(const char*);
	int(*decode_fp)(struct md_decoder_data_t*);
	int(*decode_bytes)(struct md_decoder_data_t*);
} md_decoder_ops;

typedef struct {
	const char* name;
	const char* lib;
	void* handle;
	md_decoder_ops ops;
} md_decoder_t;

__ll_define(decoder);

md_decoder_ll* md_decoder_ll_add(md_decoder_t *);
md_decoder_t* md_decoder_for(const char*);
md_decoder_t* md_decoder_ll_find(const char* name);
int md_decoder_ll_deinit(void);

#define register_decoder(_name, _decoder)			\
	__attribute__((constructor(110)))			\
	void decoder_register_##_name(void) {			\
		if (!md_decoder_ll_add(&(_decoder)))		\
			md_error("Could not add " #_name ".");	\
		else						\
			md_log("Loaded decoder " #_name ".");	\
	}

DECLARE_SYM_FUNCTIONS(decoder);

#endif
