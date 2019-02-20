#include "mdcore.h"

#define USLEEP_TIME 500

#ifdef USLEEP_TIME
#include <unistd.h>
#endif

#ifdef MDCORE_DEBUG
    #include "mdlog.h"
#endif

MD__general_t MD__general;

// private helper procedures

void        MDAL__buff_init             (MD__file_t *MD__file);
void        MDAL__pop_error             (char *message, int code);
ALenum      MDAL__get_format            (unsigned int channels, unsigned int bps);
void        MDAL__clear                 (MD__file_t *MD__file);

void        MD__remove_buffer_head      (MD__file_t *MD__file);
bool        MD__wait_if_paused          (MD__file_t *MD__file);

void        (*MD__metadata_fptr)        (MD__metadata_t, void *) = NULL;

void MD__play_raw (MD__file_t *MD__file,
                   MD__RETTYPE decoder_func (MD__ARGTYPE),
                   void (*metadata_handle) (MD__metadata_t, void *),
                   void (*playing_handle) (void *),
                   void (*error_handle) (char *, void *),
                   void (*buffer_underrun_handle) (void *),
                   void (*completion_handle) (void *)) {

    #ifdef MDCORE_DEBUG
        MD__log ("---------- MD__play called ----------");
    #endif

    if (MD__file == NULL) {

        char *message = "MD__file not initialied.";

        #ifdef MDCORE_DEBUG
            MD__log (message);
        #endif

        if (error_handle) error_handle (message, MD__file->user_data);
        return;
    }

    if ((MD__general.MD__pre_buff < MD__general.MD__buff_num) && (MD__general.MD__pre_buff != 0)) {

        char *message = "Pre-buffer must be equal or larger than number of buffers.";

        #ifdef MDCORE_DEBUG
            MD__log (message);
        #endif

        if (error_handle) error_handle (message, MD__file->user_data);

        return;
    }

    if (metadata_handle) MD__metadata_fptr = metadata_handle;

#if defined (linux) || defined (__APPLE__)

    pthread_mutex_init (&MD__file->MD__mutex, NULL);

    pthread_t decoder_thread;

    if (pthread_create (&decoder_thread, NULL, decoder_func, (void *)MD__file)) {

        char *message = "Error creating thread.";

        #ifdef MDCORE_DEBUG
            MD__log (message);
        #endif

        if (error_handle) error_handle (message, MD__file->user_data);

        #ifdef MDCORE_DEBUG
            MD__log("Closing OpenAL.");
        #endif

        MDAL__close();

        exit(41);
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
        if (error_handle) error_handle ("Error creating thread.", MD__file->user_data);
        exit(41);
    }

#endif

    #ifdef MDCORE_DEBUG
        MD__log ("Waiting for metadata to load.");
    #endif

    while (true) {

        #ifdef USLEEP_TIME
            usleep(USLEEP_TIME);
        #endif

        MD__lock (MD__file);
        if (MD__file->MD__metadata_loaded) {
            MD__unlock (MD__file);
            break;
        }
        MD__unlock (MD__file);
    }

    #ifdef MDCORE_DEBUG
        MD__log ("Metadata loaded.");
    #endif

    #ifdef MDCORE_DEBUG
        MD__log ("Waiting to fill pre-buffer.");
    #endif

    while (true) {

        #ifdef USLEEP_TIME
            usleep(USLEEP_TIME);
        #endif

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

    #ifdef MDCORE_DEBUG
        MD__log ("Pre-buffer filled.");
    #endif

    MDAL__buff_init (MD__file);

    #ifdef MDCORE_DEBUG
        MD__log ("Buffers initialized.");
        MDLOG__dynamic_reset ();
    #endif

    for (int i=0; i<MD__general.MD__buff_num; i++) {

        MD__lock (MD__file);

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Calling buffer transform closure.");
        #endif

        if (MD__file->MD__buffer_transform)

            MD__file->MD__buffer_transform (MD__file->MD__current_chunk,
                                          MD__file->MD__metadata.sample_rate,
                                          MD__file->MD__metadata.channels,
                                          MD__file->MD__metadata.bps,
                                          MD__file->user_data);

        #ifdef MDCORE_DEBUG
          MDLOG__dynamic ("Loading chunk no. %lu with size %lu (out of %lu loaded chunks).",
                          MD__file->MD__current_chunk->order, MD__file->MD__current_chunk->size,
                          MD__file->MD__last_chunk->order);
        #endif

        unsigned int mod4 = MD__file->MD__current_chunk->size % 4;

        if (mod4 != 0) MD__file->MD__current_chunk->size -= mod4;

        alBufferData (MD__file->MDAL__buffers[i], MD__file->MD__metadata.format, MD__file->MD__current_chunk->chunk,
                      MD__file->MD__current_chunk->size, (ALuint) MD__file->MD__metadata.sample_rate);

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Loaded chunk no. %lu with size %lu (out of %lu loaded chunks).",
                            MD__file->MD__current_chunk->order, MD__file->MD__current_chunk->size,
                            MD__file->MD__last_chunk->order);
        #endif

        if (MD__file->MD__current_chunk->next)

            MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;

        else if (MD__file->MD__decoding_done) {

            MD__unlock (MD__file);
            break;
        }

        MD__unlock (MD__file);
    }

    #ifdef MDCORE_DEBUG
        MD__log ("Buffers transfered to OpenAL.");
    #endif

    #ifdef MDCORE_DEBUG
        MD__log ("Queuing buffers to source.");
    #endif

    alSourceQueueBuffers (MD__file->MDAL__source, MD__general.MD__buff_num, MD__file->MDAL__buffers);

    #ifdef MDCORE_DEBUG
        MD__log ("Queued buffers.");
    #endif

    alSourcePlay (MD__file->MDAL__source);

    MD__file->MD__is_playing = true;

    #ifdef MDCORE_DEBUG
        MD__log ("Playing.");
    #endif

    if (playing_handle) playing_handle (MD__file->user_data);

    ALuint buffer;
    ALint val;

    #ifdef MDCORE_DEBUG
        MD__log ("Entering main loop.");
        MDLOG__dynamic_reset ();
    #endif

    while (true) {

        #ifdef USLEEP_TIME
            usleep(USLEEP_TIME);
        #endif

        MD__wait_if_paused (MD__file);

        MD__lock (MD__file);

        if (!MD__file->MD__current_chunk->next && !MD__file->MD__decoding_done) {

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Waiting due to buffer overhead on chunk %d.", MD__file->MD__current_chunk->order);
            #endif

            MD__unlock (MD__file);

            continue;
        }

        if ((!MD__file->MD__current_chunk->next && MD__file->MD__decoding_done)
        || MD__file->MD__stop_playing) {

            #ifdef MDCORE_DEBUG
                if (MD__file->MD__stop_playing) MD__log ("Stop signal received.");
                else MD__log ("All buffers queued, breaking loop.");
            #endif

            MD__unlock (MD__file);

            break;
        }
        MD__unlock (MD__file);

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Getting source state.");
        #endif

        alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Got source state (%d)", val);
        #endif

        if (val != AL_PLAYING) {

            #ifdef MDCORE_DEBUG
                MD__log ("Buffer underrun.");
            #endif

            if (buffer_underrun_handle) buffer_underrun_handle (MD__file->user_data);

            #ifdef MDCORE_DEBUG
                MD__log ("Attempting to resume playing.");
            #endif

            alSourcePlay (MD__file->MDAL__source);
        }

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Getting processed sources.");
        #endif

        alGetSourcei (MD__file->MDAL__source, AL_BUFFERS_PROCESSED, &val);

        if (val <= 0) {

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("No free buffers; continuing");
            #endif

            continue;
        }

        for(int i=0; i<val; i++) {

            if (MD__wait_if_paused (MD__file)) break;

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Unqueuing buffer.");
            #endif

            MD__lock (MD__file);

            alSourceUnqueueBuffers (MD__file->MDAL__source, 1, &buffer);

            if (MD__file->MD__stop_playing) {
                MD__unlock (MD__file);

                #ifdef MDCORE_DEBUG
                    MD__log ("Stop signal received during buffer load.");
                #endif

                break;
            }


            if (MD__file->MD__buffer_transform)

                MD__file->MD__buffer_transform (MD__file->MD__current_chunk,
                                              MD__file->MD__metadata.sample_rate,
                                              MD__file->MD__metadata.channels,
                                              MD__file->MD__metadata.bps,
                                              MD__file->user_data);

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Loading chunk no. %lu with size %lu (out of %lu loaded chunks).",
                                MD__file->MD__current_chunk->order, MD__file->MD__current_chunk->size,
                                MD__file->MD__last_chunk->order);
            #endif

            unsigned int mod4 = MD__file->MD__current_chunk->size % 4;

            if (mod4 != 0) MD__file->MD__current_chunk->size -= mod4;

            alBufferData (buffer, MD__file->MD__metadata.format, MD__file->MD__current_chunk->chunk,
                          MD__file->MD__current_chunk->size, (ALuint) MD__file->MD__metadata.sample_rate);

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Loaded chunk no. %lu with size %lu (out of %lu loaded chunks).",
                                MD__file->MD__current_chunk->order, MD__file->MD__current_chunk->size,
                                MD__file->MD__last_chunk->order);
            #endif

            MDAL__pop_error ("Error saving buffer data.", 22);

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Queuing buffers to source.");
            #endif

            alSourceQueueBuffers (MD__file->MDAL__source, 1, &buffer);
            MDAL__pop_error ("Error (un)queuing MD__file->MDAL__buffers.", 23);

            if (MD__file->MD__decoding_done && MD__file->MD__current_chunk->next == NULL) {
                MD__unlock (MD__file);

                #ifdef MDCORE_DEBUG
                    MD__log ("No more buffers to add, breaking the loop.");
                #endif

                break;
            }

            #ifdef MDCORE_DEBUG
                if (!MD__file->MD__current_chunk->next && !MD__file->MD__decoding_done) MD__log ("Buffer chunk overhead on no. %d.", MD__file->MD__current_chunk->order);
            #endif

            MD__unlock (MD__file);

            while (true) {

                #ifdef USLEEP_TIME
                    usleep(USLEEP_TIME);
                #endif

                MD__lock (MD__file);
                if (MD__file->MD__current_chunk->next) break;
                MD__unlock (MD__file);
            }

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Setting chunk no %lu to %lu.", MD__file->MD__current_chunk->order, MD__file->MD__current_chunk->next->order);
            #endif

            MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;
            MD__unlock (MD__file);

            MD__remove_buffer_head (MD__file);
        } // for
    } // while

    #ifdef MDCORE_DEBUG
        MD__log ("Waiting for playing to stop.");
    #endif

    while (val == AL_PLAYING) {

        #ifdef USLEEP_TIME
            usleep(USLEEP_TIME);
        #endif

        bool was_paused = MD__wait_if_paused (MD__file);

        MD__lock (MD__file);
        if (MD__file->MD__stop_playing) {

            #ifdef MDCORE_DEBUG
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

    #ifdef MDCORE_DEBUG
        MD__log ("Clearing remaining buffer data (%lu to %lu).", MD__file->MD__first_chunk->order, MD__file->MD__last_chunk->order);
    #endif

    MD__clear_buffer(MD__file);

    MDAL__clear (MD__file);

    #ifdef MDCORE_DEBUG
        MD__log ("Done playing.");
    #endif

    if (completion_handle) completion_handle (MD__file->user_data);

    #ifdef MDCORE_DEBUG
        MD__log ("Joining threads.");
    #endif

#if defined(linux) || defined(__APPLE__)
    pthread_join (decoder_thread, NULL);
#endif

#ifdef _WIN32
    WaitForSingleObject (decoder_thread, INFINITE);
#endif

}

void MD__stop_raw (MD__file_t *MD__file) {
    MD__file->MD__stop_playing = true;

    #ifdef MDCORE_DEBUG
        MD__log ("Sending stop signal...");
    #endif
}

void MD__stop (MD__file_t *MD__file) {

    MD__lock (MD__file);
    MD__file->MD__stop_playing = true;
    MD__unlock (MD__file);

    #ifdef MDCORE_DEBUG
        MD__log ("Sending stop signal...");
    #endif
}

bool MD__did_stop (MD__file_t *MD__file) {

    bool return_val = false;

    MD__lock (MD__file);
    return_val = MD__file->MD__stop_playing;
    MD__unlock (MD__file);

    return return_val;
}

bool MD__wait_if_paused (MD__file_t *MD__file) {

    MD__lock (MD__file);

        bool was_paused_or_stopped = MD__file->MD__stop_playing || MD__file->MD__pause_playing;

    MD__unlock (MD__file);


    while (true) {

        #ifdef USLEEP_TIME
            usleep(USLEEP_TIME);
        #endif

        MD__lock (MD__file);

        if (MD__file->MD__stop_playing) {

            MD__file->MD__pause_playing = false;

            MD__unlock (MD__file);

            break;
        }

        if (!MD__file->MD__pause_playing) {

            MD__unlock (MD__file);

            break;
        }
        MD__unlock (MD__file);
    }

    return was_paused_or_stopped;
}

void MD__toggle_pause_raw (MD__file_t *MD__file) {

    #ifdef MDCORE_DEBUG
        MD__log ("Received pause signal.");
    #endif

    // MD__lock (MD__file);

    if (MD__file->MD__stop_playing) {

        #ifdef MDCORE__DEBUG
            MD__log ("Ignoring pause signal received during stop process.");
        #endif

        // MD__unlock (MD__file);

        // return;
    }
    else {

        MD__file->MD__pause_playing = !MD__file->MD__pause_playing;

        ALint val;

        if (MD__file->MD__pause_playing) {

            alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);

            if (val == AL_PLAYING) {


                #ifdef MDCORE__DEBUG
                    MD__log ("Stop playing due to pause signal.");
                #endif

                alSourceStop (MD__file->MDAL__source);
            }
            else {

                #ifdef MDCORE__DEBUG
                    MD__log ("Playing already stopped.");
                #endif
            }
        }
        else {

            alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);

            if (val != AL_PLAYING) {

                alSourcePlay (MD__file->MDAL__source);
            }
            else {

                #ifdef MDCORE__DEBUG
                    MD__log ("Unpause while already playing (possible bug).");
                #endif
            }
        }
    }

    // MD__unlock (MD__file);
}

void MD__toggle_pause (MD__file_t *MD__file) {

    MD__lock (MD__file);
    MD__toggle_pause_raw (MD__file);
    MD__unlock (MD__file);
}

bool MD__is_paused (MD__file_t *MD__file) {

    bool return_val = false;

    MD__lock (MD__file);
    return_val = MD__file->MD__pause_playing;
    MD__unlock (MD__file);

    return return_val;
}

void MDAL__buff_init (MD__file_t *MD__file) {

    #ifdef MDCORE_DEBUG
        MD__log ("Allocating memory space for buffers.");
    #endif

    MD__file->MDAL__buffers = malloc (sizeof (ALuint) * MD__general.MD__buff_num);

    #ifdef MDCORE_DEBUG
        MD__log ("Generating sources.");
    #endif

    alGenSources ((ALuint)1, &MD__file->MDAL__source);
    MDAL__pop_error ("Error generating source.", 4);

    #ifdef MDCORE_DEBUG
        MD__log ("Generating buffers.");
    #endif

    alGenBuffers ((ALuint)MD__general.MD__buff_num, MD__file->MDAL__buffers);
    MDAL__pop_error ("Error creating buffer.", 5);
}


void MDAL__buff_resize (MD__file_t *MD__file, unsigned int (*resize_f) (unsigned int)) {

    // this does not really work - will have to try OpenAL close and init and see if it pays off at all

    #ifdef MDCORE_DEBUG
        MD__log ("Deleting old buffers.");
    #endif

    alDeleteBuffers ((ALuint) MD__general.MD__buff_num, MD__file->MDAL__buffers);

    #ifdef MDCORE_DEBUG
        MD__log ("Reallocating buffers.");
    #endif

    if (resize_f) MD__general.MD__buff_num = resize_f (MD__general.MD__buff_num);

    else MD__general.MD__buff_num++;

    MD__file->MDAL__buffers = realloc (MD__file->MDAL__buffers, sizeof (ALuint) * MD__general.MD__buff_num);

    #ifdef MDCORE_DEBUG
        MD__log ("Generating new buffers.");
    #endif

    alGenBuffers ((ALuint)MD__general.MD__buff_num, MD__file->MDAL__buffers);
    MDAL__pop_error ("Error creating buffers with new size.", 5);
}


void MDAL__initialize (unsigned int buffer_size,
                       unsigned int buffer_num,
                       unsigned int pre_buffer) {

    #ifdef MDCORE_DEBUG
        MD__log ("Setting MD__general variables.");
    #endif

    MD__general.MD__buff_size       = buffer_size;
    MD__general.MD__buff_num        = buffer_num;
    MD__general.MD__pre_buff        = pre_buffer;

    #ifdef MDCORE_DEBUG
        MD__log ("Opening device.");
    #endif

    MD__general.MDAL__device = alcOpenDevice(NULL);

    if (!MD__general.MDAL__device) {

        #ifdef MDCORE_DEBUG
            MD__log ("Could not open device.");
        #endif

        exit(1);
    }

    #ifdef MDCORE_DEBUG
        MD__log ("Creating context.");
    #endif

    MD__general.MDAL__context = alcCreateContext(MD__general.MDAL__device, NULL);

    if (!alcMakeContextCurrent(MD__general.MDAL__context)) {

        #ifdef MDCORE_DEBUG
            MD__log ("Error creating context.");
        #endif

        exit(2);
    }
}

void MDAL__pop_error (char *message, int code) {

    ALCenum error;

    error = alGetError();

    if (error != AL_NO_ERROR)
    {
        #ifdef MDCORE_DEBUG
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

    #ifdef MDCORE_DEBUG
        MD__log ("Deleting sources.");
    #endif

    alDeleteSources         (1, &MD__file->MDAL__source);

    #ifdef MDCORE_DEBUG
        MD__log ("Deleting buffers.");
    #endif

    alDeleteBuffers         ((ALuint) MD__general.MD__buff_num, MD__file->MDAL__buffers);

    #ifdef MDCORE_DEBUG
        MD__log ("Freeing buffers.");
    #endif

    free(MD__file->MDAL__buffers);
}

void MDAL__close () {

    #ifdef MDCORE_DEBUG
        MD__log ("Getting device context.");
    #endif

    MD__general.MDAL__device = alcGetContextsDevice (MD__general.MDAL__context);

    #ifdef MDCORE_DEBUG
        MD__log ("Current context set to NULL.");
    #endif

    alcMakeContextCurrent   (NULL);

    #ifdef MDCORE_DEBUG
        MD__log ("Destroying context.");
    #endif

    alcDestroyContext       (MD__general.MDAL__context);

    #ifdef MDCORE_DEBUG
        MD__log ("Closing device.");
    #endif

    alcCloseDevice          (MD__general.MDAL__device);
}

bool MD__initialize_seek (MD__file_t *MD__file,
                          char *filename,
                          unsigned int seek_sec) {

    bool return_val = MD__initialize (MD__file, filename);


    MD__lock (MD__file);
    MD__file->MD__seek_sec = seek_sec;
    MD__unlock (MD__file);

    return return_val;

}

bool MD__initialize (MD__file_t *MD__file, char *filename) {

    return MD__initialize_with_user_data (MD__file, filename, NULL);
}

bool MD__initialize_with_user_data (MD__file_t *MD__file, char *filename, void *user_data) {

    if (user_data) MD__file->user_data = user_data;

    MD__file->filename = filename;

    #ifdef MDCORE_DEBUG
        MD__log ("Opening file: %s.", filename);
    #endif

    MD__file->file = fopen (filename,"rb");

    if (MD__file->file == NULL) {

        #ifdef MDCORE_DEBUG
            MD__log ("Could not open file.");
        #endif

        return false;
    }

    #ifdef MDCORE_DEBUG
        MD__log ("Resetting MD__file variables.");
    #endif

    MD__file->MD__first_chunk              = NULL;
    MD__file->MD__current_chunk            = NULL;
    MD__file->MD__last_chunk               = NULL;

    MD__file->MD__metadata.sample_rate     = 0;
    MD__file->MD__metadata.channels        = 0;
    MD__file->MD__metadata.bps             = 0;
    MD__file->MD__metadata.format          = 0;

    MD__file->MD__metadata_loaded          = false;
    MD__file->MD__decoding_done            = false;

    MD__file->MD__is_playing               = false;
    MD__file->MD__stop_playing             = false;
    MD__file->MD__pause_playing            = false;

    MD__file->MD__seek_sec                 = 0;
    MD__file->MD__buffer_transform          = NULL;

    #ifdef MDCORE_DEBUG
        MD__log ("MD__file variables reset.");
    #endif

    return true;
}

void MD__clear_buffer(MD__file_t *MD__file) {

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

    #ifdef MDCORE_DEBUG
        MDLOG__dynamic ("Removing chunk no. %lu (out of %lu loaded)", old_first->order, MD__file->MD__last_chunk->order);
    #endif

    free (old_first->chunk);
    free ((void *)old_first);

    #ifdef MDCORE_DEBUG
        MDLOG__dynamic ("Head is now at %lu.", MD__file->MD__first_chunk->order);
    #endif

    MD__unlock (MD__file);

    return;
}

bool MD__add_to_buffer (MD__file_t *MD__file, unsigned char data) {

    bool result = true;

    MD__lock (MD__file);
    result = MD__add_to_buffer_raw (MD__file, data);
    MD__unlock (MD__file);

    return result;
}

bool MD__add_to_buffer_raw (MD__file_t *MD__file, unsigned char data) {

    if (MD__file->MD__stop_playing) return false;

    if ((MD__file->MD__last_chunk) && (MD__file->MD__last_chunk->size < MD__general.MD__buff_size))

        MD__file->MD__last_chunk->chunk [MD__file->MD__last_chunk->size++] = data;

    else {

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Creating new last chunk.");
        #endif

        MD__buffer_chunk_t *new_chunk = malloc (sizeof (MD__buffer_chunk_t));
        new_chunk->chunk = malloc (sizeof (unsigned char) * MD__general.MD__buff_size);
        new_chunk->size = 0;
        new_chunk->order = 0;
        new_chunk->chunk [new_chunk->size++] = data;
        new_chunk->next = NULL;

        if (MD__file->MD__last_chunk == NULL && MD__file->MD__first_chunk == NULL) {

            #ifdef MDCORE_DEBUG
                MDLOG__dynamic ("Creating first and last chunks.");
            #endif

            MD__file->MD__first_chunk = new_chunk;
            MD__file->MD__last_chunk = new_chunk;
        }
        else {

            #ifdef MDCORE_DEBUG
                long order = MD__file->MD__last_chunk->order;
                MDLOG__dynamic ("Setting new chunk as next to last (order %d).", order);
            #endif

            new_chunk->order = MD__file->MD__last_chunk->order + 1;
            MD__file->MD__last_chunk->next = new_chunk;
            MD__file->MD__last_chunk = new_chunk;
        }
    }

    return true;
}

bool MD__add_buffer_chunk_ncp (MD__file_t *MD__file, unsigned char* data, unsigned int size) {

    if (MD__did_stop (MD__file)) return false;

    MD__buffer_chunk_t *new_chunk = malloc (sizeof (MD__buffer_chunk_t));
    new_chunk->chunk = data;
    new_chunk->next = NULL;
    new_chunk->size = size;

    if (MD__file->MD__last_chunk == NULL && MD__file->MD__first_chunk == NULL) {

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Creating first and last chunks (no copy).");
        #endif

        MD__file->MD__first_chunk = new_chunk;
        MD__file->MD__last_chunk = new_chunk;
        new_chunk->order = 0;
    }
    else {

        #ifdef MDCORE_DEBUG
            MDLOG__dynamic ("Setting new chunk as next to last (order %lu, no copy).", MD__file->MD__last_chunk->order);
        #endif

        new_chunk->order = MD__file->MD__last_chunk->order + 1;
        MD__file->MD__last_chunk->next = new_chunk;
        MD__file->MD__last_chunk = new_chunk;
    }

    return true;
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

    #ifdef MDCORE_DEBUG
        MD__log ("Decoding done signal received.");
    #endif

    MD__lock (MD__file);
    MD__file->MD__decoding_done = true;
    MD__unlock (MD__file);
}

void MD__decoding_error_signal (MD__file_t *MD__file) {

    #ifdef MDCORE_DEBUG
        MD__log ("Decoding error signal received.");
    #endif

    MD__lock (MD__file);
    MD__file->MD__stop_playing = true;
    MD__unlock (MD__file);
}

bool MD__set_metadata (MD__file_t *MD__file,
                       unsigned int sample_rate,
                       unsigned int channels,
                       unsigned int bps,
                       unsigned int total_samples) {

    #ifdef MDCORE_DEBUG
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

    if (MD__metadata_fptr) MD__metadata_fptr (MD__file->MD__metadata, MD__file->user_data);

    #ifdef MDCORE_DEBUG
        MD__log ("Metadata set and dispatched.");
    #endif

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

void MD__get_logo (char logo[76]) {

    const char *logo_data = "\\          /\n |__    __|\n   _\\/\\/_\n   \\_  _/\n   | /\\ |\n ~melodeer~\n    ||||";

    for (int i=0; i<75; i++) logo[i] = logo_data[i];

    logo[75] = 0;
}
