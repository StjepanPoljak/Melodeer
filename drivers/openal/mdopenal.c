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
	bool should_be_playing;
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

OPENAL_SYMBOLS(md_define_fptr);

#define md_openal_pop_error(fmt, ...) ({				\
	ALCenum error = alGetError();					\
	error != AL_NO_ERROR						\
	      ? ({ md_error(fmt, ## __VA_ARGS__); }), error : 0;	\
})

#define md_openal_load_sym(_func) \
	md_load_sym(_func, &(openal_driver), &(error))

int md_openal_load_symbols(void) {
	int error;

	if (!openal_driver.handle) {
		md_error("OpenAL library hasn't been opened.");

		return -EINVAL;
	}

	error = 0;

	OPENAL_SYMBOLS(md_openal_load_sym);

	if (error) {
		md_error("Could not load symbols.");

		return -EINVAL;
	}

	return 0;
}

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

int md_openal_play(void) {
	int ret;

	alSourcePlay(md_openal.source);
	if ((ret = md_openal_pop_error("Cannot play.")))
		return ret;

	md_openal.should_be_playing = true;

	return 0;
}

static int md_openal_add_to_buffer(md_buf_pack_t* buf_pack) {
	md_buf_chunk_t* curr_chunk;
	int i, ret;
	bool is_playing;
	ALuint buffer;
	static bool once = false;

	curr_chunk = buf_pack->first(buf_pack);
	i = 0;

	is_playing = md_openal.should_be_playing;

	while (curr_chunk) {
/* if we're debugging and the kid is asleep, turn the volume down */
/*
		for (int j = 0; j < curr_chunk->size; j++)
			curr_chunk->chunk[j] = 0;
*/
		if (is_playing) {
			alSourceUnqueueBuffers(md_openal.source, 1, &buffer);

			if (!md_openal_is_playing()) {

				if ((ret = md_openal_play()))
					return ret;

				if (!once) {
					once = true;
					md_error("Buffer underrun.");
				}
			}
		}

		alBufferData(is_playing ? buffer : md_openal.buffers[i],
			     md_openal_get_format(curr_chunk->metadata),
			     curr_chunk->chunk, curr_chunk->size,
			     (ALuint)curr_chunk->metadata->sample_rate);
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

	if (!is_playing) {
		if ((ret = md_openal_play()))
			return ret;
	}

	md_buf_clean_pack(buf_pack);

	return 0;
}

static void* md_openal_poll_handler(void* data) {
	int val, ret;
	bool stopped;
	md_buf_pack_t* pack;

	stopped = false;

	while (!stopped) {

		alGetSourcei(md_openal.source, AL_BUFFERS_PROCESSED, &val);

		if (val <= 0 && md_openal_is_playing())
			continue;

		val = val <= 0 ? get_settings()->buf_num : val;

		ret = md_buf_get_pack(&pack, &val, MD_PACK_EXACT);
		if (ret == MD_BUF_EXIT) {
			break;
		}
		else if (ret == MD_BUF_NO_DECODERS) {
			stopped = true;
		}

		if (!val) {
			md_error("Got zero buffers.");
			break;
		}

		if ((ret = md_openal_add_to_buffer(pack)))
			break;

		if (stopped) {
			while (md_openal_is_playing()) { }

			if (md_openal_is_playing()) {
				md_error("BUG: md_openal_is_playing() == true");
			}
		}
	}

	md_log("Exiting OpenAL...");

	return NULL;
}

int md_openal_init(void) {
	int ret;

	md_openal.should_be_playing = false;

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

md_driver_state_ret_t md_openal_stop(void) {

	if (md_openal_is_playing())
		alSourceStop(md_openal.source);

	/* TODO: add drain here */

	md_openal.should_be_playing = false;

	return MD_DRIVER_STATE_SET;
}

md_driver_t openal_driver = {
	.name = "openal",
	.lib = "libopenal.so",
	.handle = NULL,
	.ops = {
		.load_symbols = md_openal_load_symbols,
		.init = md_openal_init,
		.stop = md_openal_stop,
		.pause = NULL,
		.unpause = NULL,
		.get_state = NULL,
		.deinit = md_openal_deinit
	}
};

register_driver(openal, openal_driver);
