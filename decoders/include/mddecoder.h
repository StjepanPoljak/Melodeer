#ifndef MD_DECODER_H
#define MD_DECODER_H

#include "mdll.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mdmetadata.h"
#include "mdbufchunk.h"

struct md_decoder_data_t;

typedef struct {
	bool(*decodes_file)(const char*);
	int(*decode_file)(const char*);
	int(*decode_fp)(struct md_decoder_data_t*);
	int(*decode_bytes)(uint8_t*);
} md_decoder_ops;

typedef struct {
	const char* name;
	md_decoder_ops ops;
} md_decoder_t;

struct md_decoder_ll {
	md_decoder_t* decoder;
	struct md_decoder_ll* next;
};

typedef struct md_decoder_ll md_decoder_ll;

md_decoder_ll* md_decoder_ll_add(md_decoder_t *);
md_decoder_t* md_decoder_for(const char*);
int md_decoder_ll_deinit(void);

#define register_decoder(_name, _decoder)			\
	__attribute__((constructor(110)))			\
	void decoder_register_##_name(void) {			\
		if (!md_decoder_ll_add(&(_decoder)))		\
			md_error("Could not add " #_name ".");	\
		else						\
			md_log("Loaded decoder " #_name ".");	\
	}

int md_decoder_start(const char*, const char*);

typedef struct md_decoder_data_t {
	char* fpath;
	char* force_decoder;
	md_decoder_t* fdecoder;
	md_metadata_t* metadata;
	md_buf_chunk_t* chunk;
	void* data;
} md_decoder_data_t;

int md_add_decoded_byte(md_decoder_data_t*, uint8_t);
void md_decoder_done(md_decoder_data_t*);

#define md_set_metadata(_decoder_data, _metadata) do {	\
	(_decoder_data)->metadata = (_metadata);	\
} while (0)

#endif
