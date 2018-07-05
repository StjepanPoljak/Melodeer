#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include <pthread.h>

#include <AL/al.h>
#include <AL/alc.h>

struct MD__buffer_chunk {

    unsigned char               *chunk;
    unsigned int                size;
    unsigned int                order;
    struct MD__buffer_chunk     *next;
};

struct MD__metadata {

    unsigned int    sample_rate;
    unsigned int    channels;
    unsigned int    bps;
    unsigned int    total_samples;
    unsigned int    format;
};

typedef struct      MD__buffer_chunk    MD__buffer_chunk;
typedef struct      MD__metadata        MD__metadata;

struct MD__general {

    unsigned int     MD__buff_size;
    unsigned int     MD__buff_num;
    unsigned int     MD__pre_buff;
    ALCdevice        *MDAL__device;
    ALCcontext       *MDAL__context;
};

struct MD__file {

    char *filename;

    volatile MD__buffer_chunk *MD__first_chunk;
    volatile MD__buffer_chunk *MD__current_chunk;
    volatile MD__buffer_chunk *MD__last_chunk;

    pthread_mutex_t     MD__mutex;

    MD__metadata            MD__metadata;

    volatile bool           MD__metadata_loaded;
    volatile bool           MD__decoding_done;
    volatile bool           MD__is_playing;

    volatile bool           MD__stop_playing;
    volatile bool           MD__pause_playing;

    ALuint           *MDAL__buffers;
    ALuint           MDAL__source;
};

typedef struct MD__file MD__file_t;
typedef struct MD__general MD__general_t;

void    MD__add_to_buffer           (unsigned char data);
void    MD__add_to_buffer_raw       (unsigned char data);
void    MD__add_buffer_chunk_ncp    (unsigned char* data, unsigned int size);

void    MD__initialize              ();
void    MD__clear_buffer            ();
void    MD__play                    (char *filename,
                                     void *decoder_func (void *),
                                     void (*metadata_handle) (MD__metadata metadata));

void    MD__decoding_done_signal    ();
void    MD__decoding_error_signal   ();
bool    MD__set_metadata            (unsigned int sample_rate,
                                     unsigned int channels,
                                     unsigned int bps,
                                     unsigned int total_samples);

void    MD__exit_decoder            ();

void    MDAL__initialize            (unsigned int buffer_size,
                                     unsigned int buffer_num,
                                     unsigned int pre_buffer);
void    MDAL__close                 ();

void    MD__lock                    ();
void    MD__unlock                  ();

unsigned int    MD__get_buffer_size         ();
unsigned int    MD__get_buffer_num          ();

void (*MD__buffer_transform) (volatile MD__buffer_chunk *curr_chunk,
                              unsigned int sample_rate,
                              unsigned int channels,
                              unsigned int bps);
