#include "mddriver.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>

#include "mdmetadata.h"
#include "mdbuf.h"
#include "mdlog.h"

static ALCdevice* md_openal_device;
static ALCcontext* md_openal_context;
static ALuint* md_openal_buffers;
static ALuint md_openal_source;
static pthread_t md_openal_thread;
static ALuint md_openal_buf_num;
static ALenum md_openal_format;
static ALuint md_openal_sample_rate;

static pthread_mutex_t md_openal_loop_mutex;
static bool md_openal_poll_alive;

#define md_openal_pop_error(fmt, ...) ({				\
	ALCenum error = alGetError();					\
	error != AL_NO_ERROR						\
	      ? ({ md_error(fmt, ## __VA_ARGS__); }), error : 0;	\
})

static bool md_openal_is_playing() {
	ALint val;

	alGetSourcei(md_openal_source, AL_SOURCE_STATE, &val);

	return val == AL_PLAYING;
}

static ALenum md_openal_get_format(const md_metadata_t* metadata) {

	if (metadata->bps == 8)
		return metadata->channels == 1
			? AL_FORMAT_MONO8 : (metadata->channels == 2
			? AL_FORMAT_STEREO8 : 0);
	else if (metadata->bps == 16 || metadata->bps == 24 ||
		 metadata->bps == 32)
		return metadata->channels == 1
			? AL_FORMAT_MONO16 : (metadata->channels == 2
			? AL_FORMAT_STEREO16 : 0);
	else
		return 0;
}

int md_openal_deinit(void) {

	pthread_mutex_lock(&md_openal_loop_mutex);
	md_openal_poll_alive = false;
	pthread_mutex_unlock(&md_openal_loop_mutex);

	pthread_join(md_openal_thread, NULL);

	md_log("DEINIT");

	alDeleteSources(1, &md_openal_source);

	if (md_openal_buffers) {
		alDeleteBuffers(md_openal_buf_num, md_openal_buffers);
		free(md_openal_buffers);
	}

	md_openal_device = alcGetContextsDevice(md_openal_context);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(md_openal_context);
	alcCloseDevice(md_openal_device);

	md_log("OpenAL deinitialized.");

	return 0;
}

static void md_openal_add_to_buffer(const md_buf_pack_t* buf_pack) {
	const md_buf_chunk_t* curr_chunk;
	int i;

	i = 0;
	curr_chunk = buf_pack->first(buf_pack);

	while (curr_chunk) {
		alBufferData(md_openal_buffers[i], md_openal_format,
			     curr_chunk->chunk, curr_chunk->size,
			     md_openal_sample_rate);
		alSourceQueueBuffers(md_openal_source,
				     md_openal_buf_num,
				     md_openal_buffers);
		i++;
		curr_chunk = buf_pack->next(buf_pack);
	}

	return;
}

static void* md_openal_poll_handler(void* data) {
	int val, i;
	md_buf_pack_t* pack;

	while (1) {
		pthread_mutex_lock(&md_openal_loop_mutex);
		if (!md_openal_poll_alive) {
			pthread_mutex_unlock(&md_openal_loop_mutex);
			break;
		}
		pthread_mutex_unlock(&md_openal_loop_mutex);

		alGetSourcei(md_openal_source, AL_BUFFERS_PROCESSED, &val);
		if (val <= 0)
			continue;

		md_log("Got %d buffers.", val);

		md_buf_get_pack(&pack, &val);
		if (pack)
			md_openal_add_to_buffer(pack);
	}

	return NULL;
}

int md_openal_init(void) {
	int ret;

	md_openal_device = alcOpenDevice(NULL);
	if (!md_openal_device) {
		md_error("Could not open device.");
		return -EINVAL;
	}

	md_openal_context = alcCreateContext(md_openal_device, NULL);
	if (!alcMakeContextCurrent(md_openal_context)) {
		md_error("Could not create context.");
		ret = -EINVAL;
		goto md_openal_early_fail;
	}

	alGenSources((ALuint)1, &md_openal_source);
	if ((ret = md_openal_pop_error("Could not generate sources.")))
		goto md_openal_early_fail;

	md_openal_buffers = malloc(sizeof(ALuint) * get_settings()->buf_num);
	if (!md_openal_buffers) {
		md_error("Could not allocate memory.");
		ret = -ENOMEM;
		alDeleteSources(1, &md_openal_source);
		goto md_openal_early_fail;
	}

	alGenBuffers((ALuint)get_settings()->buf_num, md_openal_buffers);
	if ((ret = md_openal_pop_error("Could not generate buffers.")))
		goto md_openal_fail;

	pthread_mutex_init(&md_openal_loop_mutex, NULL);
	md_openal_poll_alive = true;

	if (pthread_create(&md_openal_thread, NULL,
			   md_openal_poll_handler, NULL))
		goto md_openal_fail;

	return 0;

md_openal_early_fail:
	alcDestroyContext(md_openal_context);
	alcCloseDevice(md_openal_device);

	return ret;

md_openal_fail:
	(void)md_openal_deinit();

	return ret;
}

int md_openal_set_metadata(md_metadata_t* metadata) {

	if (!(md_openal_format = md_openal_get_format(metadata)))
		return -EINVAL;

	md_openal_sample_rate = (ALuint)metadata->sample_rate;

	return 0;
}

int md_openal_play(void) {

	alSourcePlay(md_openal_source);

	return !md_openal_is_playing();
}

int md_openal_stop(void) {
	ALuint buffer;

	alSourceUnqueueBuffers(md_openal_source, 1, &buffer);
	alSourceStop (md_openal_source);

	return 0;
}

md_driver_t openal_driver = {
	.name = "openal",
	.ops = {
		.init = md_openal_init,
		.set_metadata = md_openal_set_metadata,
		.play = md_openal_play,
		.stop = md_openal_stop,
		.pause = NULL,
		.get_state = NULL,
		.deinit = md_openal_deinit
	}
};

register_driver(openal, openal_driver);
