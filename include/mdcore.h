#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include <AL/al.h>
#include <AL/alc.h>

void    MD__add_to_buffer           (unsigned char data);
void    MD__add_to_buffer_raw       (unsigned char data);
void    MD__add_buffer_chunk_ncp    (unsigned char* data, unsigned int size);

void    MD__initialize              ();
void    MD__clear_buffer            ();
void    MD__play                    (char *filename,
                                     void *decoder_func (void *),
                                     void (*metadata_handle) (unsigned int sample_rate,
                                                              unsigned int channels,
                                                              unsigned int bps,
                                                              unsigned int total_samples));
void    MD__decoding_done_signal    ();
void    MD__decoding_error_signal   ();
bool    MD__set_metadata            (unsigned int sample_rate,
                                     unsigned int channels,
                                     unsigned int bps,
                                     unsigned int total_samples);

void    MD__exit_decoder            ();

void    MDAL__initialize            ();
void    MDAL__close                 ();

void    MD__lock                    ();
void    MD__unlock                  ();

unsigned int    MD__get_buffer_size         ();
unsigned int    MD__get_buffer_num          ();
