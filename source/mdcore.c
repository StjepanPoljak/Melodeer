#include "mdcore.h"

#ifdef MDCORE__DEBUG

    #include <sys/time.h>

    #define SEC_PER_DAY   86400
    #define SEC_PER_HOUR  3600
    #define SEC_PER_MIN   60

#endif

MD__general_t MD__general;

void    MDAL__buff_init             (MD__file_t *MD__file);
void    MDAL__pop_error             (char *message, int code);
ALenum  MDAL__get_format            (unsigned int channels, unsigned int bps);
void    MD__remove_buffer_head      (MD__file_t *MD__file);
void    MDAL__clear                 (MD__file_t *MD__file);

void (*MD__metadata_fptr)       (MD__metadata_t) = NULL;

bool logging = true;

void MD__log (char *string) {

#ifdef MDCORE__DEBUG

    FILE *f;
    f = fopen ("mdcore.log", "a");
    if (f == NULL) return;

    time_t t = time (NULL);
    struct tm tm = *localtime (&t);
    
    fprintf (f, "[%02d.%02d.%04d. %02d:%02d:%02d] : %s\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, string);
    
    fclose (f);

#endif
}

void MD__play (MD__file_t *MD__file, MD__RETTYPE decoder_func (MD__ARGTYPE),
               void (*metadata_handle) (MD__metadata_t), void (*playing_handle)(),
               void (*error_handle) (char *), void (*completion) (void)) {

    if (MD__file == NULL) {
        
        char *message = "Could not initialize file.";

        #ifdef MDCORE__DEBUG
            MD__log (message);
        #endif

        error_handle (message);
        return;
    }

    if ((MD__general.MD__pre_buff < MD__general.MD__buff_num) && (MD__general.MD__pre_buff != 0)) {

        char *message = "Pre-buffer must be equal or larger than number of buffers.";

        #ifdef MDCORE__DEBUG
            MD__log (message);
        #endif

        error_handle (message);

        return;
    }

    MD__metadata_fptr = metadata_handle;

#if defined(linux) || defined(__APPLE__)
    pthread_mutex_init (&MD__file->MD__mutex, NULL);

    pthread_t decoder_thread;

    if (pthread_create (&decoder_thread, NULL, decoder_func, (void *)MD__file)) {

        char *message = "Error creating thread.";

        #ifdef MDCORE__DEBUG
            MD__log (message);
        #endif

        error_handle (message);

        #ifdef MDCORE__DEBUG
            MD__log("Closing OpenAL.");
        #endif

        MDAL__close();

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

    #ifdef MDCORE__DEBUG
        MD__log ("Waiting for metadata to load.");
    #endif

    while (true) {

        MD__lock (MD__file);
        if (MD__file->MD__metadata_loaded) {
            MD__unlock (MD__file);
            break;
        }
        MD__unlock (MD__file);
    }

    #ifdef MDCORE__DEBUG
        MD__log ("Metadata loaded.");
    #endif

    #ifdef MDCORE__DEBUG
        MD__log ("Waiting to fill pre-buffer.");
    #endif

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

    #ifdef MDCORE__DEBUG
        MD__log ("Pre-buffer filled.");
    #endif

    MDAL__buff_init (MD__file);

    #ifdef MDCORE__DEBUG
        MD__log ("Buffers initialized.");
    #endif

    for(int i=0; i<MD__general.MD__buff_num; i++) {

        MD__lock (MD__file);
        if (MD__file->MD__buffer_transform != NULL) {

            MD__file->MD__buffer_transform (MD__file->MD__current_chunk,
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

    #ifdef MDCORE__DEBUG
        MD__log ("Buffers transfered to OpenAL.");
    #endif

    #ifdef MDCORE__DEBUG
        MD__log ("Enqueuing buffers.");
    #endif

    alSourceQueueBuffers (MD__file->MDAL__source, MD__general.MD__buff_num, MD__file->MDAL__buffers);

    #ifdef MDCORE__DEBUG
        MD__log ("Attempting to play.");
    #endif

    alSourcePlay (MD__file->MDAL__source);

    MD__file->MD__is_playing = true;

    #ifdef MDCORE__DEBUG
        MD__log ("Playing.");
    #endif

    playing_handle();

    ALuint buffer;
    ALint val;

    while (true)
    {
        MD__lock (MD__file);

        // the && !MD__file->MD__stop_playing, etc... is only to make signal fall through to if below
        if ((MD__file->MD__current_chunk->next == NULL && MD__file->MD__decoding_done)
        || MD__file->MD__stop_playing) {

            #ifdef MDCORE__DEBUG
                if (MD__file->MD__stop_playing) MD__log ("Stop signal received.");
                else MD__log ("All buffers queued, breaking loop.");
            #endif

            MD__unlock (MD__file);

            break;
        }
        MD__unlock (MD__file);

        alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);

        if(val != AL_PLAYING) {

            #ifdef MDCORE__DEBUG
                MD__log ("Buffer underrun.");
            #endif

            // will make a special closure for this
            error_handle ("Buffer underrun.");

            #ifdef MDCORE__DEBUG
                MD__log ("Attempting to resume playing.");
            #endif

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

            if (MD__file->MD__buffer_transform != NULL) {

                MD__file->MD__buffer_transform (MD__file->MD__current_chunk,
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

                #ifdef MDCORE__DEBUG
                    MD__log ("No more buffers to add, breaking the loop.");
                #endif

                break;
            }

            MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;
            MD__unlock (MD__file);

            MD__remove_buffer_head (MD__file);
        } // for
    } // while

    #ifdef MDCORE__DEBUG
        MD__log ("Waiting for playing to stop.");
    #endif


    while (val == AL_PLAYING) {

        MD__lock (MD__file);
        if (MD__file->MD__stop_playing) {

            #ifdef MDCORE__DEBUG
                MD__log ("There was a stop signal - playing forcibly stopped.");
            #endif

            alSourceStop (MD__file->MDAL__source);
            MD__unlock (MD__file);

            break;
        }
        else {

            alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);
        }
        MD__unlock (MD__file);
    }

    #ifdef MDCORE__DEBUG
        MD__log ("Clearing remaining buffer data.");
    #endif

    MD__clear_buffer(MD__file);

    MDAL__clear (MD__file);

    completion();

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

    #ifdef MDCORE__DEBUG
        MD__log ("Allocating memory space for buffers.");
    #endif

    MD__file->MDAL__buffers = malloc (sizeof (ALuint) * MD__general.MD__buff_num);

    #ifdef MDCORE__DEBUG
        MD__log ("Generating sources.");
    #endif

    alGenSources((ALuint)1, &MD__file->MDAL__source);
    MDAL__pop_error("Error generating source.", 4);

    #ifdef MDCORE__DEBUG
        MD__log ("Generating buffers.");
    #endif

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
        #ifdef MDCORE__DEBUG
            MD__log ("Could not open device.");
        #endif
        
        exit(1);
    }

    MD__general.MDAL__context = alcCreateContext(MD__general.MDAL__device, NULL);

    if (!alcMakeContextCurrent(MD__general.MDAL__context))
    {
        #ifdef MDCORE__DEBUG
            MD__log ("Error creating context.");
        #endif

        exit(2);
    }
}

void MDAL__pop_error (char *message, int code)
{
    ALCenum error;

    error = alGetError();

    if (error != AL_NO_ERROR)
    {
        #ifdef MDCORE__DEBUG
            MD__log (message);
        #endif

        MDAL__close ();

        exit (code);
    }

    return;
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

    #ifdef MDCORE__DEBUG
        MD__log ("Deleting sources.");
    #endif

    alDeleteSources         (1, &MD__file->MDAL__source);

    #ifdef MDCORE__DEBUG
        MD__log ("Deleting buffers.");
    #endif

    alDeleteBuffers         ((ALuint) MD__general.MD__buff_num, MD__file->MDAL__buffers);

    #ifdef MDCORE__DEBUG
        MD__log ("Freeing buffers.");
    #endif

    free(MD__file->MDAL__buffers);
}

void MDAL__close () {

    #ifdef MDCORE__DEBUG
        MD__log ("Getting device context.");
    #endif

    MD__general.MDAL__device = alcGetContextsDevice (MD__general.MDAL__context);

    #ifdef MDCORE__DEBUG
        MD__log ("Current context set to NULL.");
    #endif

    alcMakeContextCurrent   (NULL);

    #ifdef MDCORE__DEBUG
        MD__log ("Destroying context.");
    #endif

    alcDestroyContext       (MD__general.MDAL__context);

    #ifdef MDCORE__DEBUG
        MD__log ("Closing device.");
    #endif

    alcCloseDevice          (MD__general.MDAL__device);
}

bool MD__initialize (MD__file_t *MD__file, char *filename) {

    MD__file->filename = filename;

    #ifdef MDCORE__DEBUG
        MD__log ("Opening file.");
    #endif

    MD__file->file = fopen(filename,"rb");

    if (MD__file->file == NULL) {

        #ifdef MDCORE__DEBUG
            MD__log ("Could not open file.");
        #endif

        return false;
    }

    #ifdef MDCORE__DEBUG
        MD__log ("Resetting MD__file variables.");
    #endif


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

    #ifdef MDCORE__DEBUG
        MD__log ("MD__file variables reset.");
    #endif


    return true;
}

void MD__clear_buffer(MD__file_t *MD__file)
{
    while (MD__file->MD__current_chunk != NULL) {

        volatile MD__buffer_chunk_t * volatile to_be_freed = MD__file->MD__current_chunk;
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

    volatile MD__buffer_chunk_t *old_first = MD__file->MD__first_chunk;

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
        MD__buffer_chunk_t *new_chunk = malloc (sizeof (MD__buffer_chunk_t));
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
    MD__buffer_chunk_t *new_chunk = malloc (sizeof (MD__buffer_chunk_t));
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

    #ifdef MDCORE__DEBUG
        MD__log ("Decoding done signal received.");
    #endif

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

    #ifdef MDCORE__DEBUG
        MD__log ("Setting metadata.");
    #endif

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
