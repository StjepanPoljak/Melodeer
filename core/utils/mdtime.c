#include "mdtime.h"

unsigned long md_buff_bytes_per_sec(md_metadata_t* metadata, int buff_size) {

	return (metadata->bps / 8) * metadata->channels
	     * (metadata->sample_rate);
}

double md_buff_usec(md_metadata_t* metadata, int buff_size) {

	return (buff_size * 1000000)
	     / (double)md_buff_bytes_per_sec(metadata, buff_size);
}

double md_buff_msec(md_metadata_t* metadata, int buff_size) {

	return (buff_size * 1000)
	     / (double)md_buff_bytes_per_sec(metadata, buff_size);
}

double md_buff_sec(md_metadata_t* metadata, int buff_size) {

	return buff_size
	     / (double)md_buff_bytes_per_sec(metadata, buff_size);
}
