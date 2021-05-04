#ifndef MD_METADATA_H
#define MD_METADATA_H

#include <stdint.h>

typedef struct {
	char*		fname;
	int		sample_rate;
	uint8_t		channels;
	uint16_t	bps;
	int		total_samples;
	int		total_buf_chunks;
} md_metadata_t;

#endif
