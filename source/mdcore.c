#include "mdcore.h"

MD__general_t MD__general;

void    MDAL__buff_init             (MD__file_t *MD__file);
int     MDAL__pop_error             (char *message, int code);
ALenum  MDAL__get_format            (unsigned int channels, unsigned int bps);
void    MD__remove_buffer_head      (MD__file_t *MD__file);
void    MDAL__clear                 (MD__file_t *MD__file);

void (*MD__metadata_fptr)       (MD__metadata) = NULL;
void (*MD__buffer_transform)    (volatile MD__buffer_chunk *,
                                 unsigned int sample_rate,
                                 unsigned int channels,
                                 unsigned int bps) = NULL;

void MD__play (MD__file_t *MD__file, MD__RETTYPE decoder_func (MD__ARGTYPE),
               void (*metadata_handle) (MD__metadata), void (*playing_handle)(),
               void (*error_handle) (char *), void (*completion) (void)) {

    if (MD__file == NULL) {
        error_handle("File not initialized.");
        return;
    }

    if ((MD__general.MD__pre_buff < MD__general.MD__buff_num) && (MD__general.MD__pre_buff != 0)) {

        error_handle("Pre-buffer must be equal or larger than number of buffers.");
        return;
    }

    MD__metadata_fptr = metadata_handle;

#if defined(linux) || defined(__APPLE__)
    pthread_mutex_init (&MD__file->MD__mutex, NULL);

    pthread_t decoder_thread;

    if (pthread_create (&decoder_thread, NULL, decoder_func, (void *)MD__file))
    {
        MDAL__close();
        error_handle ("Error creating thread.");
        return;
    }
#endif

#ifdef _WIN32
    MD__file->MD__mutex = CreateMutex (NULL, FALSE, NULL);

    DWORD WINAPI decoder_w32wrapper (LPVOID arg) {
        decoder_func((void *)arg);
        return 0;
    }

    HANDLE decoder_thread = CreateThread (NULL, 0, decoder_w32wrapper, (LPVOID) MD__file, 0, NULL);

    if (!decoder_thread) {

        MDAL__close ();
        error_handle ("Error creating thread.");
        exit(41);
    }
#endif

    while (true) {

        MD__lock (MD__file);
        if (MD__file->MD__metadata_loaded) {
            MD__unlock (MD__file);
            break;
        }
        MD__unlock (MD__file);
    }

    while (true) {

        MD__lock (MD__file);
        if (MD__file->MD__last_chunk != NULL) {

            if (MD__general.MD__pre_buff == 0) {

                if (MD__file->MD__decoding_done) {

                    MD__file->MD__current_chunk = MD__file->MD__first_chunk;
                    MD__unlock (MD__file);
                    break;
                }

                MD__unlock (MD__file);
                continue;
            }

            if (MD__file->MD__last_chunk->order > MD__general.MD__pre_buff - 1 || MD__file->MD__decoding_done) {

                MD__file->MD__current_chunk = MD__file->MD__first_chunk;
                MD__unlock (MD__file);
                break;
            }
        }
        MD__unlock (MD__file);
    }

    MDAL__buff_init (MD__file);

    for(int i=0; i<MD__general.MD__buff_num; i++) {

        MD__lock (MD__file);
        if (MD__buffer_transform != NULL) {

            MD__buffer_transform (MD__file->MD__current_chunk,
                                  MD__file->MD__metadata.sample_rate,
                                  MD__file->MD__metadata.channels,
                                  MD__file->MD__metadata.bps);
        }

        alBufferData (MD__file->MDAL__buffers[i], MD__file->MD__metadata.format, MD__file->MD__current_chunk->chunk,
                      MD__file->MD__current_chunk->size, (ALuint) MD__file->MD__metadata.sample_rate);

        if (MD__file->MD__current_chunk->next != NULL) {

            MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;
        }
        else if (MD__file->MD__decoding_done) {
            MD__unlock (MD__file);
            break;
        }
        MD__unlock (MD__file);
    }

    alSourceQueueBuffers (MD__file->MDAL__source, MD__general.MD__buff_num, MD__file->MDAL__buffers);
    alSourcePlay (MD__file->MDAL__source);

    // error_handle ("Playing...");
    MD__file->MD__is_playing = true;

    playing_handle();

    ALuint buffer;
    ALint val;

    while (true)
    {

        MD__lock (MD__file);

        // the && !MD__file->MD__stop_playing, etc... is only to make signal fall through to if below
        if ((MD__file->MD__current_chunk->next == NULL && MD__file->MD__decoding_done)
        || MD__file->MD__stop_playing) {
            MD__unlock (MD__file);
            break;
        }
        MD__unlock (MD__file);

        alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);

        if(val != AL_PLAYING) {

            error_handle ("Buffer underrun.");

            alSourcePlay (MD__file->MDAL__source);
        }

        alGetSourcei (MD__file->MDAL__source, AL_BUFFERS_PROCESSED, &val);

        if (val <= 0) {

            continue;
        }

        for(int i=0; i<val; i++) {
            alSourceUnqueueBuffers (MD__file->MDAL__source, 1, &buffer);

            MD__lock (MD__file);

            if (MD__file->MD__stop_playing) {
                MD__unlock (MD__file);
                break;
            }

            if (MD__buffer_transform != NULL) {

                MD__buffer_transform (MD__file->MD__current_chunk,
                                      MD__file->MD__metadata.sample_rate,
                                      MD__file->MD__metadata.channels,
                                      MD__file->MD__metadata.bps);
            }
            alBufferData (buffer, MD__file->MD__metadata.format, MD__file->MD__current_chunk->chunk,
                          MD__file->MD__current_chunk->size, (ALuint) MD__file->MD__metadata.sample_rate);
            MDAL__pop_error ("Error saving buffer data.", 22);

            alSourceQueueBuffers (MD__file->MDAL__source, 1, &buffer);
            MDAL__pop_error ("Error (un)queuing MD__file->MDAL__buffers.", 23);

            if (MD__file->MD__decoding_done && MD__file->MD__current_chunk->next == NULL) {
                MD__unlock (MD__file);
                break;
            }

            MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;
            MD__unlock (MD__file);

            MD__remove_buffer_head (MD__file);
        } // for
    } // while

    while (val == AL_PLAYING) {

        MD__lock (MD__file);
        if (MD__file->MD__stop_playing) {
            alSourceStop (MD__file->MDAL__source);
            MD__unlock (MD__file);
            break;
        }
        else {
            alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);
        }
        MD__unlock (MD__file);
    }

    MD__clear_buffer(MD__file);

    MDAL__clear (MD__file);

MDLABEL__end:

    completion();
    // error_handle ("Done playing.");


#if defined(linux) || defined(__APPLE__)
    pthread_join (decoder_thread, NULL);
#endif

#ifdef _WIN32
    WaitForSingleObject(decoder_thread, INFINITE);
#endif

}

void MD__stop (MD__file_t *MD__file) {

    MD__lock (MD__file);
    MD__file->MD__stop_playing = true;
    MD__unlock (MD__file);
}

bool MD__did_stop (MD__file_t *MD__file) {

    bool return_val = false;

    MD__lock (MD__file);
    return_val = MD__file->MD__stop_playing;
    MD__unlock (MD__file);

    return return_val;
}

void MDAL__buff_init (MD__file_t *MD__file) {

    MD__file->MDAL__buffers = malloc (sizeof (ALuint) * MD__general.MD__buff_num);

    alGenSources((ALuint)1, &MD__file->MDAL__source);
    MDAL__pop_error("Error generating source.", 4);

    alGenBuffers((ALuint)MD__general.MD__buff_num, MD__file->MDAL__buffers);
    MDAL__pop_error("Error creating buffer.", 5);
}

void MDAL__initialize (unsigned int buffer_size,
                       unsigned int buffer_num,
                       unsigned int pre_buffer) {

    MD__general.MD__buff_size       = buffer_size;
    MD__general.MD__buff_num        = buffer_num;
    MD__general.MD__pre_buff        = pre_buffer;

    MD__general.MDAL__device = alcOpenDevice(NULL);

    if (!MD__general.MDAL__device)
    {
        printf("Cannot open device.\n");
        exit(1);
    }

    MD__general.MDAL__context = alcCreateContext(MD__general.MDAL__device, NULL);

    if (!alcMakeContextCurrent(MD__general.MDAL__context))
    {
        printf("Error creating context.\n");
        exit(2);
    }
}

int MDAL__pop_error (char *message, int code)
{
    ALCenum error;

    error = alGetError();

    if (error != AL_NO_ERROR)
    {
        MDAL__close ();
        printf ("OpenAL error: %s\n", message);
        exit (code);
    }

    return code;
}

ALenum MDAL__get_format (unsigned int channels, unsigned int bps) {

    ALenum format = 0;

    if (bps == 8) {

        if (channels == 1) {

            format = AL_FORMAT_MONO8;

        } else if (channels == 2) {

            format = AL_FORMAT_STEREO8;
        }
    }
    else if (bps == 16 || bps == 24 || bps == 32) {

        if (channels == 1) {

            format = AL_FORMAT_MONO16;

        } else if (channels == 2) {

            format = AL_FORMAT_STEREO16;
        }
    }

    return format;
}

void MDAL__clear (MD__file_t *MD__file) {

    alDeleteSources         (1, &MD__file->MDAL__source);
    alDeleteBuffers         ((ALuint) MD__general.MD__buff_num, MD__file->MDAL__buffers);

    free(MD__file->MDAL__buffers);
}

void MDAL__close () {

    MD__general.MDAL__device = alcGetContextsDevice (MD__general.MDAL__context);
    alcMakeContextCurrent   (NULL);
    alcDestroyContext       (MD__general.MDAL__context);
    alcCloseDevice          (MD__general.MDAL__device);
}

bool MD__initialize (MD__file_t *MD__file, char *filename) {

    MD__file->filename = filename;
    MD__file->file = fopen(filename,"rb");

    if (MD__file->file == NULL) {
        // printf(" (!) Could not open file %s\n", filename);
        return false;
    }

    MD__file->MD__first_chunk     = NULL;
    MD__file->MD__current_chunk   = NULL;
    MD__file->MD__last_chunk      = NULL;

    MD__file->MD__metadata.sample_rate     = 0;
    MD__file->MD__metadata.channels        = 0;
    MD__file->MD__metadata.bps             = 0;
    MD__file->MD__metadata.format          = 0;

    MD__file->MD__metadata_loaded = false;
    MD__file->MD__decoding_done   = false;
    MD__file->MD__is_playing      = false;

    MD__file->MD__stop_playing    = false;
    MD__file->MD__pause_playing   = false;

    return true;
}

void MD__clear_buffer(MD__file_t *MD__file)
{
    while (MD__file->MD__current_chunk != NULL) {

        volatile MD__buffer_chunk * volatile to_be_freed = MD__file->MD__current_chunk;
        MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;

        free (to_be_freed->chunk);
        free ((void *)to_be_freed);
    }
}


void MD__remove_buffer_head (MD__file_t *MD__file) {

    MD__lock (MD__file);

    if (MD__file->MD__first_chunk == NULL) {

        MD__unlock (MD__file);
        return;
    }

    volatile MD__buffer_chunk *old_first = MD__file->MD__first_chunk;

    if (MD__file->MD__first_chunk == MD__file->MD__last_chunk) {

        MD__file->MD__last_chunk = NULL;
    }

    MD__file->MD__first_chunk = MD__file->MD__first_chunk->next;

    free (old_first->chunk);
    free ((void *)old_first);

    MD__unlock (MD__file);

    return;
}

void MD__add_to_buffer (MD__file_t *MD__file, unsigned char data) {

    MD__lock (MD__file);
    MD__add_to_buffer_raw (MD__file, data);
    MD__unlock (MD__file);
}

void MD__add_to_buffer_raw (MD__file_t *MD__file, unsigned char data) {

    if ((MD__file->MD__last_chunk != NULL) && (MD__file->MD__last_chunk->size < MD__general.MD__buff_size)) {

            MD__file->MD__last_chunk->chunk [MD__file->MD__last_chunk->size++] = data;
    }
    else
    {
        MD__buffer_chunk *new_chunk = malloc (sizeof (MD__buffer_chunk));
        new_chunk->chunk = malloc (sizeof (unsigned char) * MD__general.MD__buff_size);
        new_chunk->size = 0;
        new_chunk->order = 0;
        new_chunk->chunk [new_chunk->size++] = data;
        new_chunk->next = NULL;

        if (MD__file->MD__last_chunk == NULL && MD__file->MD__first_chunk == NULL) {

            MD__file->MD__first_chunk = new_chunk;
            MD__file->MD__last_chunk = new_chunk;
        }
        else {

            new_chunk->order = MD__file->MD__last_chunk->order + 1;
            MD__file->MD__last_chunk->next = new_chunk;
            MD__file->MD__last_chunk = new_chunk;
        }
    }
}

void MD__add_buffer_chunk_ncp (MD__file_t *MD__file, unsigned char* data, unsigned int size)
{
    MD__buffer_chunk *new_chunk = malloc (sizeof (MD__buffer_chunk));
    new_chunk->chunk = data;
    new_chunk->next = NULL;
    new_chunk->size = size;

    if (MD__file->MD__last_chunk == NULL && MD__file->MD__first_chunk == NULL) {

        MD__file->MD__first_chunk = new_chunk;
        MD__file->MD__last_chunk = new_chunk;
        new_chunk->order = 0;
    }
    else {

        new_chunk->order = MD__file->MD__last_chunk->order + 1;
        MD__file->MD__last_chunk->next = new_chunk;
        MD__file->MD__last_chunk = new_chunk;
    }
}


unsigned int MD__get_buffer_size (MD__file_t *MD__file) {

    MD__lock (MD__file);
    unsigned int buff_temp_size = MD__general.MD__buff_size;
    MD__unlock (MD__file);

    return buff_temp_size;
}

unsigned int MD__get_buffer_num (MD__file_t *MD__file) {

    MD__lock (MD__file);
    unsigned int buff_temp_num = MD__general.MD__buff_num;
    MD__unlock (MD__file);

    return buff_temp_num;
}

void MD__decoding_done_signal (MD__file_t *MD__file) {

    MD__lock (MD__file);
    MD__file->MD__decoding_done = true;
    MD__unlock (MD__file);
}

void MD__decoding_error_signal (MD__file_t *MD__file) {

    MD__lock (MD__file);
    MD__file->MD__stop_playing = true;
    MD__unlock (MD__file);
}

bool MD__set_metadata (MD__file_t *MD__file,
                       unsigned int sample_rate,
                       unsigned int channels,
                       unsigned int bps,
                       unsigned int total_samples) {

    MD__lock (MD__file);
    MD__file->MD__metadata.sample_rate        = sample_rate;
    MD__file->MD__metadata.channels           = channels;
    MD__file->MD__metadata.bps                = bps;
    MD__file->MD__metadata.total_samples      = total_samples;
    MD__file->MD__metadata.format             = MDAL__get_format (channels, bps);

    if (MD__file->MD__metadata.format) {

        MD__file->MD__metadata_loaded     = true;
    }
    else {

        MD__file->MD__stop_playing        = true;
        MD__unlock (MD__file);

        return false;
    }

    MD__metadata_fptr (MD__file->MD__metadata);

    MD__unlock (MD__file);

    return true;
}

void MD__exit_decoder() {

    #if defined(linux) || defined(__APPLE__)
    pthread_exit (NULL);
    #endif

    #ifdef _WIN32
    _endthread ();
    #endif
}

void MD__lock (MD__file_t *MD__file) {

    #if defined(linux) || defined(__APPLE__)
    pthread_mutex_lock (&MD__file->MD__mutex);
    #endif

    #ifdef _WIN32
    WaitForSingleObject (MD__file->MD__mutex, INFINITE);
    #endif

}

void MD__unlock (MD__file_t *MD__file) {

    #if defined(linux) || defined(__APPLE__)
    pthread_mutex_unlock (&MD__file->MD__mutex);
    #endif

    #ifdef _WIN32
    ReleaseMutex (MD__file->MD__mutex);
    #endif

}
