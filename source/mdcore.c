#include "mdcore.h"

struct MD__buffer_chunk {

    unsigned char               *chunk;
    unsigned int                size;
    unsigned int                order;
    struct MD__buffer_chunk     *next;

} MD__buffer_chunk_init = { NULL, 0, 0, NULL };

typedef struct          MD__buffer_chunk MD__buffer_chunk;
typedef struct          MD__metadata MD__metadata;

static volatile         MD__buffer_chunk *volatile MD__first_chunk;
static volatile         MD__buffer_chunk *volatile MD__current_chunk;
static volatile         MD__buffer_chunk *volatile MD__last_chunk;

pthread_mutex_t         MD__mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int     MD__buff_size;
static unsigned int     MD__buff_num;
static unsigned int     MD__pre_buff;

static unsigned         MD__sample_rate;
static unsigned         MD__channels;
static unsigned         MD__bps;
static ALenum           MD__format;

volatile bool           MD__metadata_loaded;
volatile bool           MD__decoding_done;
volatile bool           MD__decoding_error;
volatile bool           MD__is_playing;

volatile bool           MD__stop_playing;
volatile bool           MD__pause_playing;

static ALCdevice        *MDAL__device;
static ALCcontext       *MDAL__context;
static ALuint           *MDAL__buffers;
static ALuint           MDAL__source;

void    MDAL__buff_init             ();
int     MDAL__pop_error             (char *message, int code);
ALenum  MDAL__get_format            (unsigned int channels, unsigned int bps);
void    MD__remove_buffer_head      ();
void    MDAL__clear                 ();

void (*MD__metadata_fptr) (unsigned int, unsigned int, unsigned int, unsigned int) = NULL;

void MD__play (char *filename, void *decoder_func (void *),
               void (*metadata_handle) (unsigned int,
                                        unsigned int,
                                        unsigned int,
                                        unsigned int)) {

    MD__metadata_fptr = metadata_handle;

    pthread_t decoder_thread;

    if (pthread_create (&decoder_thread, NULL, decoder_func, (void *)filename))
    {
        MDAL__close();
        printf ("Error creating thread.\n");
        exit (40);
    }

    while (true) {

        pthread_mutex_lock (&MD__mutex);
        if (MD__metadata_loaded) {
            pthread_mutex_unlock (&MD__mutex);
            break;
        }
        pthread_mutex_unlock (&MD__mutex);
    }

    while (true) {

        pthread_mutex_lock (&MD__mutex);
        if (MD__decoding_error) {
            pthread_mutex_unlock (&MD__mutex);
            break;
        }

        if (MD__last_chunk != NULL) {

            if (MD__pre_buff == 0) {
                if (MD__decoding_done) {
                    MD__current_chunk = MD__first_chunk;
                    pthread_mutex_unlock (&MD__mutex);
                    break;
                }

                pthread_mutex_unlock (&MD__mutex);
                continue;
            }

            if (MD__last_chunk->order > MD__pre_buff - 1 || MD__decoding_done) {

                MD__current_chunk = MD__first_chunk;
                pthread_mutex_unlock (&MD__mutex);
                break;
            }
        }
        pthread_mutex_unlock (&MD__mutex);
    }

    pthread_mutex_lock (&MD__mutex);
    if (MD__decoding_error) {
        pthread_mutex_unlock (&MD__mutex);

        pthread_join (decoder_thread, NULL);
    }
    pthread_mutex_unlock (&MD__mutex);

    MDAL__buff_init ();

    for(int i=0; i<MD__buff_num; i++) {

        pthread_mutex_lock (&MD__mutex);
        alBufferData (MDAL__buffers[i],
                      MD__format,
                      MD__current_chunk->chunk,
                      MD__current_chunk->size,
                      (ALuint) MD__sample_rate);

        if (MD__current_chunk->next != NULL) {

            MD__current_chunk = MD__current_chunk->next;
        }
        else if (MD__decoding_done) {
            pthread_mutex_unlock (&MD__mutex);
            break;
        }
        pthread_mutex_unlock (&MD__mutex);
    }

    alSourceQueueBuffers (MDAL__source, MD__buff_num, MDAL__buffers);
    alSourcePlay (MDAL__source);

    printf("Playing...\n");

    MD__is_playing = true;

    while (true)
    {
        pthread_mutex_lock(&MD__mutex);
        // the && !MD__stop_playing, etc... is only to make signal fall through to if below
        if (MD__current_chunk == NULL && !MD__stop_playing && !MD__decoding_error) {

            pthread_mutex_unlock (&MD__mutex);
            continue;
        }

        if ((MD__is_playing && MD__current_chunk->next == NULL && MD__decoding_done)
        || MD__stop_playing || MD__decoding_error) {
        pthread_mutex_unlock (&MD__mutex);

            ALint val;

            alGetSourcei (MDAL__source, AL_SOURCE_STATE, &val);

            if (val != AL_PLAYING || MD__stop_playing) {

                // we shouldn't need locks here anymore
                MD__clear_buffer();
                MD__initialize ();

                MDAL__clear ();
                printf("Done playing.\n");
                break;
            }

            continue;
        }

        if (MD__current_chunk->next == NULL) {
            pthread_mutex_unlock (&MD__mutex);

            continue;
        }
        pthread_mutex_unlock (&MD__mutex);

        ALuint buffer;
        ALint val;

        alGetSourcei (MDAL__source, AL_SOURCE_STATE, &val);

        if(val != AL_PLAYING) {

            printf("Buffer underrun.\n");

            alSourcePlay (MDAL__source);
        }

        alGetSourcei (MDAL__source, AL_BUFFERS_PROCESSED, &val);

        if (val <= 0 && MD__is_playing) {

            continue;
        }

        for(int i=0; i<val; i++) {

            alSourceUnqueueBuffers (MDAL__source, 1, &buffer);

            pthread_mutex_lock (&MD__mutex);
            alBufferData (buffer, MD__format, MD__current_chunk->chunk,
                          MD__current_chunk->size, (ALuint) MD__sample_rate);
            MDAL__pop_error ("Error saving buffer data.", 22);
            pthread_mutex_unlock (&MD__mutex);

            alSourceQueueBuffers (MDAL__source, 1, &buffer);
            MDAL__pop_error ("Error (un)queuing MDAL__buffers.", 22);

            pthread_mutex_lock (&MD__mutex);
            if (MD__decoding_done && MD__current_chunk->next == NULL) {
                pthread_mutex_unlock (&MD__mutex);
                break;
            }

            MD__current_chunk = MD__current_chunk->next;
            pthread_mutex_unlock (&MD__mutex);

            MD__remove_buffer_head ();
        }

        pthread_mutex_lock (&MD__mutex);
        if (MD__decoding_done && MD__current_chunk->next == NULL) {

            pthread_mutex_unlock (&MD__mutex);
            continue;
        }
        pthread_mutex_unlock (&MD__mutex);


    }

    pthread_join (decoder_thread, NULL);

}

void MDAL__buff_init () {

    MDAL__buffers = malloc (sizeof (ALuint) * MD__buff_num);

    alGenSources((ALuint)1, &MDAL__source);
    MDAL__pop_error("Error generating source.", 4);

    alGenBuffers((ALuint)MD__buff_num, MDAL__buffers);
    MDAL__pop_error("Error creating buffer.", 5);
}


void MDAL__initialize ()
{
    MDAL__device = alcOpenDevice(NULL);

    if (!MDAL__device)
    {
        printf("Cannot open device.\n");
        exit(1);
    }

    MDAL__context = alcCreateContext(MDAL__device, NULL);

    if (!alcMakeContextCurrent(MDAL__context))
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

    alDeleteSources         (1, &MDAL__source);
    alDeleteBuffers         ((ALuint) MD__buff_num, MDAL__buffers);
}

void MDAL__close () {

    MDAL__device = alcGetContextsDevice (MDAL__context);

    free(MDAL__buffers);

    alcMakeContextCurrent   (NULL);
    alcDestroyContext       (MDAL__context);
    alcCloseDevice          (MDAL__device);
}

void MD__initialize ()
{
    MD__first_chunk     = NULL;
    MD__current_chunk   = NULL;
    MD__last_chunk      = NULL;

    MD__buff_size       = 8192;
    MD__buff_num        = 3;
    MD__pre_buff        = 3;

    MD__sample_rate     = 0;
    MD__channels        = 0;
    MD__bps             = 0;
    MD__format          = 0;

    MD__metadata_loaded = false;
    MD__decoding_done   = false;
    MD__decoding_error  = false;
    MD__is_playing      = false;

    MD__stop_playing    = false;
    MD__pause_playing   = false;
}

void MD__clear_buffer()
{
    while (MD__current_chunk != NULL) {

        volatile MD__buffer_chunk * volatile to_be_freed = MD__current_chunk;
        MD__current_chunk = MD__current_chunk->next;

        free (to_be_freed->chunk);
        free ((void *)to_be_freed);
    }
}


void MD__remove_buffer_head () {

    pthread_mutex_lock (&MD__mutex);

    if (MD__first_chunk == NULL) {

        pthread_mutex_unlock (&MD__mutex);
        return;
    }

    volatile MD__buffer_chunk *old_first = MD__first_chunk;

    if (MD__first_chunk == MD__last_chunk) {

        MD__last_chunk = NULL;
    }

    MD__first_chunk = MD__first_chunk->next;

    pthread_mutex_unlock (&MD__mutex);

    free (old_first->chunk);
    free ((void *)old_first);

    return;
}

void MD__add_to_buffer (unsigned char data) {

    pthread_mutex_lock (&MD__mutex);
    MD__add_to_buffer_raw (data);
    pthread_mutex_unlock (&MD__mutex);
}

void MD__add_to_buffer_raw (unsigned char data) {

    if ((MD__last_chunk != NULL) && (MD__last_chunk->size < MD__buff_size)) {

            MD__last_chunk->chunk [MD__last_chunk->size++] = data;
    }
    else
    {
        MD__buffer_chunk *new_chunk = malloc (sizeof (MD__buffer_chunk));
        new_chunk->chunk = malloc (sizeof (unsigned char) * MD__buff_size);
        new_chunk->size = 0;
        new_chunk->order = 0;
        new_chunk->chunk [new_chunk->size++] = data;
        new_chunk->next = NULL;

        if (MD__last_chunk == NULL && MD__first_chunk == NULL) {

            MD__first_chunk = new_chunk;
            MD__last_chunk = new_chunk;
        }
        else {

            new_chunk->order = MD__last_chunk->order + 1;
            MD__last_chunk->next = new_chunk;
            MD__last_chunk = new_chunk;
        }
    }
}

void MD__add_buffer_chunk_ncp (unsigned char* data, unsigned int size)
{
    MD__buffer_chunk *new_chunk = malloc (sizeof (MD__buffer_chunk));
    new_chunk->chunk = data;
    new_chunk->next = NULL;
    new_chunk->size = size;

    if (MD__last_chunk == NULL && MD__first_chunk == NULL) {

        MD__first_chunk = new_chunk;
        MD__last_chunk = new_chunk;
        new_chunk->order = 0;
    }
    else {

        new_chunk->order = MD__last_chunk->order + 1;
        MD__last_chunk->next = new_chunk;
        MD__last_chunk = new_chunk;
    }
}


unsigned int MD__get_buffer_size () {

    pthread_mutex_lock (&MD__mutex);
    unsigned int buff_temp_size = MD__buff_size;
    pthread_mutex_unlock (&MD__mutex);

    return buff_temp_size;
}

unsigned int MD__get_buffer_num () {

    pthread_mutex_lock (&MD__mutex);
    unsigned int buff_temp_num = MD__buff_num;
    pthread_mutex_unlock (&MD__mutex);

    return buff_temp_num;
}

void MD__decoding_done_signal () {

    pthread_mutex_lock (&MD__mutex);
    MD__decoding_done = true;
    pthread_mutex_unlock (&MD__mutex);
}

void MD__decoding_error_signal () {

    pthread_mutex_lock (&MD__mutex);
    MD__decoding_error = true;
    pthread_mutex_unlock (&MD__mutex);
}

bool MD__set_metadata (unsigned int sample_rate,
                       unsigned int channels,
                       unsigned int bps,
                       unsigned int total_samples) {

    pthread_mutex_lock (&MD__mutex);
    MD__sample_rate         = sample_rate;
    MD__channels            = channels;
    MD__format              = MDAL__get_format (channels, bps);

    if (MD__format) {

        MD__metadata_loaded     = true;
    }
    else {

        MD__decoding_error      = true;
        pthread_mutex_unlock (&MD__mutex);

        return false;
    }

    MD__metadata_fptr (sample_rate, channels, bps, total_samples);

    pthread_mutex_unlock (&MD__mutex);

    return true;
}

void MD__exit_decoder() {

    pthread_exit (NULL);
}

void MD__lock () {

    pthread_mutex_lock (&MD__mutex);
}

void MD__unlock () {

    pthread_mutex_unlock (&MD__mutex);
}
