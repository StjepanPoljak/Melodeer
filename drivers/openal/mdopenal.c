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
#include "mdgarbage.h"

md_driver_t openal_driver;

static struct md_openal_t {
	ALCdevice* device;
	ALCcontext* context;
	ALuint* buffers;
	ALuint source;
	pthread_t thread;
	ALenum format;
	ALuint sample_rate;
} md_openal;

#define OPENAL_SYMBOLS(FUN)		\
	FUN(alGetSourcei);		\
	FUN(alDeleteSources);		\
	FUN(alDeleteBuffers);		\
	FUN(alcGetContextsDevice);	\
	FUN(alcMakeContextCurrent);	\
	FUN(alcDestroyContext);		\
	FUN(alcCloseDevice);		\
	FUN(alSourceUnqueueBuffers);	\
	FUN(alBufferData);		\
	FUN(alSourceQueueBuffers);	\
	FUN(alSourcePlay);		\
	FUN(alSourceStop);		\
	FUN(alcOpenDevice);		\
	FUN(alcCreateContext);		\
	FUN(alGenSources);		\
	FUN(alGenBuffers);		\
	FUN(alGetError);

OPENAL_SYMBOLS(md_driver_define_fptr);

#define alGetSourcei alGetSourcei_ptr
#define alDeleteSources alDeleteSources_ptr
#define alDeleteBuffers alDeleteBuffers_ptr
#define alcGetContextsDevice alcGetContextsDevice_ptr
#define alcMakeContextCurrent alcMakeContextCurrent_ptr
#define alcDestroyContext alcDestroyContext_ptr
#define alcCloseDevice alcCloseDevice_ptr
#define alSourceUnqueueBuffers alSourceUnqueueBuffers_ptr
#define alBufferData alBufferData_ptr
#define alSourceQueueBuffers alSourceQueueBuffers_ptr
#define alSourcePlay alSourcePlay_ptr
#define alSourceStop alSourceStop_ptr
#define alcOpenDevice alcOpenDevice_ptr
#define alcCreateContext alcCreateContext_ptr
#define alGenSources alGenSources_ptr
#define alGenBuffers alGenBuffers_ptr
#define alGetError alGetError_ptr

#define md_openal_pop_error(fmt, ...) ({				\
	ALCenum error = alGetError();					\
	error != AL_NO_ERROR						\
	      ? ({ md_error(fmt, ## __VA_ARGS__); }), error : 0;	\
})

#define md_openal_load_sym(_func) \
	md_driver_load_sym(_func ##_ptr, &(openal_driver), error)

int md_openal_load_symbols(void) {
	char** error;

	if (!openal_driver.handle) {
		md_error("OpenAL library hasn't been opened.");

		return -EINVAL;
	}

	*error = 0;

	OPENAL_SYMBOLS(md_openal_load_sym);

	if (*error) {
		md_error("Could not load symbol: %s", *error);
		free(*error);

		return -EINVAL;
	}

	return 0;
}

static bool md_openal_is_playing() {
	ALint val;

	alGetSourcei(md_openal.source, AL_SOURCE_STATE, &val);

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

	pthread_join(md_openal.thread, NULL);

	alDeleteSources(1, &md_openal.source);

	if (md_openal.buffers) {
		alDeleteBuffers(get_settings()->buf_num, md_openal.buffers);
		free(md_openal.buffers);
	}

	md_openal.device = alcGetContextsDevice(md_openal.context);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(md_openal.context);
	alcCloseDevice(md_openal.device);

	md_log("OpenAL deinitialized.");

	return 0;
}

static int md_openal_add_to_buffer(md_buf_pack_t* buf_pack,
				   int total, int get_pack_ret) {
	md_buf_chunk_t* curr_chunk;
	int i, ret;
	bool is_playing;
	ALuint buffer;

	curr_chunk = buf_pack->first(buf_pack);
	i = 0;

	is_playing = md_openal_is_playing();

	while (curr_chunk) {

		ret = md_driver_exec_events(curr_chunk);
		if (ret) {
			md_error("Error executing events.");
			return ret;
		}

		if (is_playing)
			alSourceUnqueueBuffers(md_openal.source, 1, &buffer);

		alBufferData(is_playing ? buffer : md_openal.buffers[i],
			     md_openal.format,
			     curr_chunk->chunk, curr_chunk->size,
			     md_openal.sample_rate);
		if ((ret = md_openal_pop_error("Could not set buffer data.")))
			return ret;

		alSourceQueueBuffers(md_openal.source, 1,
				     is_playing ? &buffer
						: &(md_openal.buffers[i]));
		if ((ret = md_openal_pop_error("Could not queue buffers.")))
			return ret;

		i++;
		curr_chunk = buf_pack->next(buf_pack);
	}

	if (!is_playing)
		alSourcePlay(md_openal.source);

	md_buf_clean_pack(buf_pack);

	return 0;
}

static void* md_openal_poll_handler(void* data) {
	int val, i, ret;
	md_buf_pack_t* pack;

	while (1) {

		alGetSourcei(md_openal.source, AL_BUFFERS_PROCESSED, &val);

		if (val <= 0 && md_openal_is_playing())
			continue;

		val = val <= 0 ? get_settings()->buf_num : val;

		ret = md_buf_get_pack(&pack, &val, MD_PACK_EXACT);
		if (ret == MD_BUF_EXIT) {
			md_log("Exiting OpenAL...");
			break;
		}

		if ((ret = md_openal_add_to_buffer(pack, val, ret)))
			break;
	}

	return NULL;
}

int md_openal_init(void) {
	int ret;

	md_openal.device = alcOpenDevice(NULL);
	if (!md_openal.device) {
		md_error("Could not open device.");
		return -EINVAL;
	}

	md_openal.context = alcCreateContext(md_openal.device, NULL);
	if (!alcMakeContextCurrent(md_openal.context)) {
		md_error("Could not create context.");
		ret = -EINVAL;
		goto md_openal_early_fail;
	}

	alGenSources((ALuint)1, &md_openal.source);
	if ((ret = md_openal_pop_error("Could not generate sources.")))
		goto md_openal_early_fail;

	md_openal.buffers = malloc(sizeof(ALuint) * get_settings()->buf_num);
	if (!md_openal.buffers) {
		md_error("Could not allocate memory.");
		ret = -ENOMEM;
		alDeleteSources(1, &md_openal.source);
		goto md_openal_early_fail;
	}

	alGenBuffers(get_settings()->buf_num, md_openal.buffers);
	if ((ret = md_openal_pop_error("Could not generate buffers.")))
		goto md_openal_fail;

	md_log("Initialized OpenAL.");

	if (pthread_create(&md_openal.thread, NULL,
			   md_openal_poll_handler, NULL))
		goto md_openal_fail;

	return 0;

md_openal_early_fail:
	alcDestroyContext(md_openal.context);
	alcCloseDevice(md_openal.device);

	return ret;

md_openal_fail:
	(void)md_openal_deinit();

	return ret;
}

int md_openal_set_metadata(md_metadata_t* metadata) {

	if (!(md_openal.format = md_openal_get_format(metadata))) {
		md_error("Unsupported PCM format.");
		return -EINVAL;
	}

	md_openal.sample_rate = (ALuint)metadata->sample_rate;

	return 0;
}

int md_openal_play(void) {

	alSourcePlay(md_openal.source);

	return 0;
}

int md_openal_stop(void) {

	alSourceStop(md_openal.source);

	return 0;
}

md_driver_t openal_driver = {
	.name = "openal",
	.lib = "openal",
	.handle = NULL,
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
