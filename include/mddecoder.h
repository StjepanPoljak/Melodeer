#ifndef MD_DECODER_H
#define MD_DECODER_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
	int(*init)(void);
	int(*decode_file)(const char*);
	int(*decode_fp)(const FILE*);
	int(*decode_bytes)(const uint8_t*);
	int(*deinit)(void);
} md_decoder_ops;

typedef struct {
	const char* name;
	md_decoder_ops ops;
} md_decoder_t;

#endif
