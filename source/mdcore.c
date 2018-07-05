#include "mdcore.h"

MD__general_t MD__general;
MD__file_t *MD__file = NULL;

void    MDAL__buff_init             ();
int     MDAL__pop_error             (char *message, int code);
ALenum  MDAL__get_format            (unsigned int channels, unsigned int bps);
void    MD__remove_buffer_head      ();
void    MDAL__clear                 ();

void (*MD__metadata_fptr)       (MD__metadata) = NULL;
void (*MD__buffer_transform)    (volatile MD__buffer_chunk *,
                                 unsigned int sample_rate,
                                 unsigned int channels,
                                 unsigned int bps) = NULL;

void MD__play (char *filename, void *decoder_func (void *),
               void (*metadata_handle) (MD__metadata)) {

    MD__file = (MD__file_t *)malloc (sizeof (MD__file_t));

    MD__initialize ();

    pthread_mutex_init(&MD__file->MD__mutex, NULL);

    struct stat attr_buff;
    if (stat( filename, &attr_buff ) == -1) {

        printf ("Can't open file.\n");
        return;
    }

    if ((MD__general.MD__pre_buff < MD__general.MD__buff_num) && (MD__general.MD__pre_buff != 0)) {

        printf("Pre-buffer must be equal or larger than number of buffers.\n");
        return;
    }

    MD__metadata_fptr = metadata_handle;

    pthread_t decoder_thread;

    if (pthread_create (&decoder_thread, NULL, decoder_func, (void *)filename))
    {
        MDAL__close();
        printf ("Error creating thread.\n");
        exit (40);
    }

    while (true) {

        pthread_mutex_lock (&MD__file->MD__mutex);
        if (MD__file->MD__stop_playing) {

            pthread_mutex_unlock (&MD__file->MD__mutex);
            return;
        }

        if (MD__file->MD__metadata_loaded) {
            pthread_mutex_unlock (&MD__file->MD__mutex);
            break;
        }
        pthread_mutex_unlock (&MD__file->MD__mutex);
    }

    while (true) {

        pthread_mutex_lock (&MD__file->MD__mutex);
        if (MD__file->MD__stop_playing) {
            pthread_mutex_unlock (&MD__file->MD__mutex);
            break;
        }

        if (MD__file->MD__last_chunk != NULL) {

            if (MD__general.MD__pre_buff == 0) {

                if (MD__file->MD__decoding_done) {

                    MD__file->MD__current_chunk = MD__file->MD__first_chunk;
                    pthread_mutex_unlock (&MD__file->MD__mutex);
                    break;
                }

                pthread_mutex_unlock (&MD__file->MD__mutex);
                continue;
            }

            if (MD__file->MD__last_chunk->order > MD__general.MD__pre_buff - 1 || MD__file->MD__decoding_done) {

                MD__file->MD__current_chunk = MD__file->MD__first_chunk;
                pthread_mutex_unlock (&MD__file->MD__mutex);
                break;
            }
        }
        pthread_mutex_unlock (&MD__file->MD__mutex);
    }

    pthread_mutex_lock (&MD__file->MD__mutex);
    if (MD__file->MD__stop_playing) {
        pthread_mutex_unlock (&MD__file->MD__mutex);

        pthread_join (decoder_thread, NULL);
    }
    pthread_mutex_unlock (&MD__file->MD__mutex);

    MDAL__buff_init ();

    for(int i=0; i<MD__general.MD__buff_num; i++) {

        pthread_mutex_lock (&MD__file->MD__mutex);
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
            pthread_mutex_unlock (&MD__file->MD__mutex);
            break;
        }
        pthread_mutex_unlock (&MD__file->MD__mutex);
    }

    alSourceQueueBuffers (MD__file->MDAL__source, MD__general.MD__buff_num, MD__file->MDAL__buffers);
    alSourcePlay (MD__file->MDAL__source);

    printf("Playing...\n");

    ALuint buffer;
    ALint val;
    MD__file->MD__is_playing = true;

    while (true)
    {
        pthread_mutex_lock(&MD__file->MD__mutex);
        // the && !MD__file->MD__stop_playing, etc... is only to make signal fall through to if below
        if ((MD__file->MD__current_chunk->next == NULL && MD__file->MD__decoding_done)
        || MD__file->MD__stop_playing) {

            pthread_mutex_unlock (&MD__file->MD__mutex);
            break;
        }
        pthread_mutex_unlock (&MD__file->MD__mutex);

        alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);

        if(val != AL_PLAYING) {

            printf("Buffer underrun.\n");

            alSourcePlay (MD__file->MDAL__source);
        }

        alGetSourcei (MD__file->MDAL__source, AL_BUFFERS_PROCESSED, &val);

        if (val <= 0) {

            continue;
        }

        for(int i=0; i<val; i++) {

            alSourceUnqueueBuffers (MD__file->MDAL__source, 1, &buffer);

            pthread_mutex_lock (&MD__file->MD__mutex);
            if (MD__buffer_transform != NULL) {

                MD__buffer_transform (MD__file->MD__current_chunk,
                                      MD__file->MD__metadata.sample_rate,
                                      MD__file->MD__metadata.channels,
                                      MD__file->MD__metadata.bps);
            }
            alBufferData (buffer, MD__file->MD__metadata.format, MD__file->MD__current_chunk->chunk,
                          MD__file->MD__current_chunk->size, (ALuint) MD__file->MD__metadata.sample_rate);
            pthread_mutex_unlock (&MD__file->MD__mutex);
            MDAL__pop_error ("Error saving buffer data.", 22);

            alSourceQueueBuffers (MD__file->MDAL__source, 1, &buffer);
            MDAL__pop_error ("Error (un)queuing MD__file->MDAL__buffers.", 23);

            pthread_mutex_lock (&MD__file->MD__mutex);
            if (MD__file->MD__decoding_done && MD__file->MD__current_chunk->next == NULL) {
                pthread_mutex_unlock (&MD__file->MD__mutex);
                break;
            }

            MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;
            pthread_mutex_unlock (&MD__file->MD__mutex);

            MD__remove_buffer_head ();
        }
    }

    while(val == AL_PLAYING) {

        alGetSourcei (MD__file->MDAL__source, AL_SOURCE_STATE, &val);
    }

    MD__clear_buffer();
    MD__initialize (MD__general.MD__buff_size, MD__general.MD__buff_num, MD__general.MD__pre_buff);

    MDAL__clear ();
    printf("Done playing.\n");
    free(MD__file);

    pthread_join (decoder_thread, NULL);

}

void MDAL__buff_init () {

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
        printf("Error creating context.");
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

void MDAL__clear () {

    alDeleteSources         (1, &MD__file->MDAL__source);
    alDeleteBuffers         ((ALuint) MD__general.MD__buff_num, MD__file->MDAL__buffers);
}

void MDAL__close () {

    MD__general.MDAL__device = alcGetContextsDevice (MD__general.MDAL__context);

    free(MD__file->MDAL__buffers);

    alcMakeContextCurrent   (NULL);
    alcDestroyContext       (MD__general.MDAL__context);
    alcCloseDevice          (MD__general.MDAL__device);
}

void MD__initialize () {

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
}

void MD__clear_buffer()
{
    while (MD__file->MD__current_chunk != NULL) {

        volatile MD__buffer_chunk * volatile to_be_freed = MD__file->MD__current_chunk;
        MD__file->MD__current_chunk = MD__file->MD__current_chunk->next;

        free (to_be_freed->chunk);
        free ((void *)to_be_freed);
    }
}


void MD__remove_buffer_head () {

    pthread_mutex_lock (&MD__file->MD__mutex);

    if (MD__file->MD__first_chunk == NULL) {

        pthread_mutex_unlock (&MD__file->MD__mutex);
        return;
    }

    volatile MD__buffer_chunk *old_first = MD__file->MD__first_chunk;

    if (MD__file->MD__first_chunk == MD__file->MD__last_chunk) {

        MD__file->MD__last_chunk = NULL;
    }

    MD__file->MD__first_chunk = MD__file->MD__first_chunk->next;

    pthread_mutex_unlock (&MD__file->MD__mutex);

    free (old_first->chunk);
    free ((void *)old_first);

    return;
}

void MD__add_to_buffer (unsigned char data) {

    pthread_mutex_lock (&MD__file->MD__mutex);
    MD__add_to_buffer_raw (data);
    pthread_mutex_unlock (&MD__file->MD__mutex);
}

void MD__add_to_buffer_raw (unsigned char data) {

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

void MD__add_buffer_chunk_ncp (unsigned char* data, unsigned int size)
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


unsigned int MD__get_buffer_size () {

    pthread_mutex_lock (&MD__file->MD__mutex);
    unsigned int buff_temp_size = MD__general.MD__buff_size;
    pthread_mutex_unlock (&MD__file->MD__mutex);

    return buff_temp_size;
}

unsigned int MD__get_buffer_num () {

    pthread_mutex_lock (&MD__file->MD__mutex);
    unsigned int buff_temp_num = MD__general.MD__buff_num;
    pthread_mutex_unlock (&MD__file->MD__mutex);

    return buff_temp_num;
}

void MD__decoding_done_signal () {

    pthread_mutex_lock (&MD__file->MD__mutex);
    MD__file->MD__decoding_done = true;
    pthread_mutex_unlock (&MD__file->MD__mutex);
}

void MD__decoding_error_signal () {

    pthread_mutex_lock (&MD__file->MD__mutex);
    MD__file->MD__stop_playing = true;
    pthread_mutex_unlock (&MD__file->MD__mutex);
}

bool MD__set_metadata (unsigned int sample_rate,
                       unsigned int channels,
                       unsigned int bps,
                       unsigned int total_samples) {

    pthread_mutex_lock (&MD__file->MD__mutex);
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
        pthread_mutex_unlock (&MD__file->MD__mutex);

        return false;
    }

    MD__metadata_fptr (MD__file->MD__metadata);

    pthread_mutex_unlock (&MD__file->MD__mutex);

    return true;
}

void MD__exit_decoder() {

    pthread_exit (NULL);
}

void MD__lock () {

    pthread_mutex_lock (&MD__file->MD__mutex);
}

void MD__unlock () {

    pthread_mutex_unlock (&MD__file->MD__mutex);
}
