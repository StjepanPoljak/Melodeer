#ifndef MDUTILS_H

#include <math.h>
#include <complex.h>

#include "mdcore.h"

enum MD__filetype { MD__FLAC, MD__WAV, MD__MP3, MD__UNKNOWN };

typedef enum MD__filetype MD__filetype;

MD__filetype MD__get_extension (const char *filename);

MD__filetype MD__get_filetype (const char *filename);

unsigned int MD__get_curr_seconds (MD__file_t *MD__file);

void MDFFT__iterative (bool inverse, float complex v_in[], float complex v_out[], int count);

void MDFFT__to_amp (float complex v_in[], float v_out[], int count);

void MDFFT__to_amp_surj (float complex v_in[], unsigned int count_in,
                         float v_out[], unsigned int count_out);

void tests ();

#endif

#define MDUTILS_H
