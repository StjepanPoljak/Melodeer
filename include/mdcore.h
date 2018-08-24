#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
typedef LPVOID MD__ARGTYPE;
typedef DWORD WINAPI MD__RETTYPE;
#endif

#if defined(linux) || defined(__APPLE__)
#include <pthread.h>
typedef void * MD__ARGTYPE;
typedef void * MD__RETTYPE;
#endif

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
    FILE *file;

    volatile MD__buffer_chunk *MD__first_chunk;
    volatile MD__buffer_chunk *MD__current_chunk;
    volatile MD__buffer_chunk *MD__last_chunk;

#if defined(linux) || defined(__APPLE__)
    pthread_mutex_t     MD__mutex;
#endif

#ifdef _WIN32
    HANDLE              MD__mutex;
#endif

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

void    MD__add_to_buffer           (MD__file_t *MD__file, unsigned char data);
void    MD__add_to_buffer_raw       (MD__file_t *MD__file, unsigned char data);
void    MD__add_buffer_chunk_ncp    (MD__file_t *MD__file, unsigned char* data, unsigned int size);

bool    MD__initialize              (MD__file_t *MD__file, char *filename);
void    MD__clear_buffer            (MD__file_t *MD__file);

void    MD__play                    (MD__file_t *MD__file,
                                     MD__RETTYPE decoder_func (MD__ARGTYPE),
                                     void (*metadata_handle) (MD__metadata metadata),
                                     void (*playing_handle) (void),
                                     void (*error_handle) (char *info),
                                     void (*completion) (void));

void    MD__stop                     (MD__file_t *MD__file);

void    MD__decoding_done_signal    (MD__file_t *MD__file);
void    MD__decoding_error_signal   (MD__file_t *MD__file);
bool    MD__set_metadata            (MD__file_t *MD__file,
                                     unsigned int sample_rate,
                                     unsigned int channels,
                                     unsigned int bps,
                                     unsigned int total_samples);

void    MD__exit_decoder            ();
void    MD__deinit                  (MD__file_t *MD__file);

void    MDAL__initialize            (unsigned int buffer_size,
                                     unsigned int buffer_num,
                                     unsigned int pre_buffer);
void    MDAL__close                 ();

void    MD__lock                    (MD__file_t *MD__file);
void    MD__unlock                  (MD__file_t *MD__file);

unsigned int    MD__get_buffer_size         (MD__file_t *MD__file);
unsigned int    MD__get_buffer_num          (MD__file_t *MD__file);

void (*MD__buffer_transform) (volatile MD__buffer_chunk *curr_chunk,
                              unsigned int sample_rate,
                              unsigned int channels,
                              unsigned int bps);
