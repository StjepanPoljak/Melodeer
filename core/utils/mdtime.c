#include "mdtime.h"

unsigned long md_buf_len_usec(md_metadata_t* metadata, int buf_size) {

	/* max bps: 24
	 * max channels: 8
	 * max sample_rate: 192.000
	 * -----------------------
	 * == 36.864.000 */

	return ((unsigned long)buf_size * 8 * 1000000)
	     / (metadata->bps * metadata->channels * metadata->sample_rate);
}

double md_buf_len_sec(md_metadata_t* metadata, int buf_size) {

	return ((double)buf_size * 8)
	     / (metadata->bps * metadata->channels * metadata->sample_rate);
}
