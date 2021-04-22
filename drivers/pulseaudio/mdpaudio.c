#include "mddriver.h"

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>	// memset
#include <pulse/pulseaudio.h>
#include <pthread.h>

#include "mdmetadata.h"
#include "mdbuf.h"
#include "mdlog.h"
#include "mdgarbage.h"

md_driver_t paudio_driver;

#define PAUDIO_SYMBOLS(FUN)				\
	FUN(pa_mainloop_new);				\
	FUN(pa_mainloop_get_api);			\
	FUN(pa_mainloop_run);				\
	FUN(pa_context_new);				\
	FUN(pa_context_connect);			\
	FUN(pa_context_set_state_callback);		\
	FUN(pa_context_get_state);			\
	FUN(pa_stream_new);				\
	FUN(pa_strerror);				\
	FUN(pa_context_errno);				\
	FUN(pa_stream_set_state_callback);		\
	FUN(pa_stream_set_write_callback);		\
	FUN(pa_stream_set_suspended_callback);		\
	FUN(pa_stream_set_moved_callback);		\
	FUN(pa_stream_set_underflow_callback);		\
	FUN(pa_stream_set_overflow_callback);		\
	FUN(pa_stream_set_started_callback);		\
	FUN(pa_stream_set_event_callback);		\
	FUN(pa_stream_set_buffer_attr_callback);	\
	FUN(pa_stream_connect_playback);		\
	FUN(pa_stream_trigger);				\
	FUN(pa_stream_write);				\
	FUN(pa_stream_get_state);			\
	FUN(pa_stream_drain);				\
	FUN(pa_operation_unref);			\
	FUN(pa_stream_disconnect);			\
	FUN(pa_context_disconnect);			\
	FUN(pa_context_drain);				\
	FUN(pa_stream_unref);

PAUDIO_SYMBOLS(md_define_fptr);

#define md_paudio_load_sym(_func) \
	md_load_sym(_func, &(paudio_driver), &(error))

int md_paudio_load_symbols(void) {
	int error;

	if (!paudio_driver.handle) {
		md_error("OpenAL library hasn't been opened.");

		return -EINVAL;
	}

	error = 0;

	PAUDIO_SYMBOLS(md_paudio_load_sym);

	if (error) {
		md_error("Could not load symbols.");

		return -EINVAL;
	}

	return 0;
}

#define pa_mainloop_new pa_mainloop_new_ptr
#define pa_mainloop_get_api pa_mainloop_get_api_ptr
#define pa_mainloop_run pa_mainloop_run_ptr
#define pa_context_new pa_context_new_ptr
#define pa_context_connect pa_context_connect_ptr
#define pa_context_set_state_callback pa_context_set_state_callback_ptr
#define pa_context_get_state pa_context_get_state_ptr
#define pa_stream_new pa_stream_new_ptr
#define pa_strerror pa_strerror_ptr
#define pa_context_errno pa_context_errno_ptr
#define pa_stream_set_state_callback pa_stream_set_state_callback_ptr
#define pa_stream_set_write_callback pa_stream_set_write_callback_ptr
#define pa_stream_set_suspended_callback pa_stream_set_suspended_callback_ptr
#define pa_stream_set_moved_callback pa_stream_set_moved_callback_ptr
#define pa_stream_set_underflow_callback pa_stream_set_underflow_callback_ptr
#define pa_stream_set_overflow_callback pa_stream_set_overflow_callback_ptr
#define pa_stream_set_started_callback pa_stream_set_started_callback_ptr
#define pa_stream_set_event_callback pa_stream_set_event_callback_ptr
#define pa_stream_set_buffer_attr_callback	\
	pa_stream_set_buffer_attr_callback_ptr
#define pa_stream_connect_playback pa_stream_connect_playback_ptr
#define pa_stream_trigger pa_stream_trigger_ptr
#define pa_stream_write pa_stream_write_ptr
#define pa_stream_get_state pa_stream_get_state_ptr
#define pa_stream_drain pa_stream_drain_ptr
#define pa_operation_unref pa_operation_unref_ptr
#define pa_stream_disconnect pa_stream_disconnect_ptr
#define pa_context_disconnect pa_context_disconnect_ptr
#define pa_context_drain pa_context_drain_ptr
#define pa_stream_unref pa_stream_unref_ptr

static struct md_paudio_t {
	pa_mainloop* mainloop;
	pa_mainloop_api* mainloop_api;
	pa_context* context;
	pa_operation* operation;
	pa_sample_spec sample_spec;
	pa_buffer_attr buffer_attr;
	pa_stream_flags_t stream_flags;
	pa_stream* stream;
	pthread_t paudio_thread;
} md_paudio;

static void md_pa_get_sample_spec(md_metadata_t* metadata,
				  pa_sample_spec* sample_spec) {
	pa_sample_format_t format;

	switch (metadata->bps) {
	case 16:
		format = PA_SAMPLE_S16LE;
		break;
	case 24:
		format = PA_SAMPLE_S24LE;
		break;
	case 32:
		format = PA_SAMPLE_S32LE;
		break;
	default:
		format = PA_SAMPLE_INVALID;
		break;
	}

	sample_spec->channels = metadata->channels;
	sample_spec->rate = metadata->sample_rate;
	sample_spec->format = format;

	return;
}

static int md_paudio_add_to_buffer(md_buf_pack_t* buf_pack,
				   pa_stream* stream) {
	md_buf_chunk_t* curr_chunk;
	int i;

	curr_chunk = buf_pack->first(buf_pack);
	i = 0;

	while (curr_chunk) {

/* if we're debugging and the kid is asleep, turn the volume down */
/*
		for (int j = 0; j < curr_chunk->size; j++)
			curr_chunk->chunk[j] = 0;
*/
		if (pa_stream_write(stream, curr_chunk->chunk,
				    curr_chunk->size, NULL,
				    0, PA_SEEK_RELATIVE) < 0) {
			md_error("Could not write to Pulseaudio: %s",
				 pa_strerror(pa_context_errno(
						md_paudio.context)));
			return -EINVAL;
		}

		i++;
		curr_chunk = buf_pack->next(buf_pack);
	}

	md_buf_clean_pack(buf_pack);

	return 0;
}

static void md_pa_stream_state_cb(pa_stream* stream, void* data) {
	md_log("Stream state changed.");

	switch (pa_stream_get_state(stream)) {
	case PA_STREAM_CREATING:
		md_log("Creating state...");
		break;
	case PA_STREAM_TERMINATED:
		md_log("Terminated stream...");
		break;
	case PA_STREAM_READY:
		md_log("Stream ready...");
		break;
	case PA_STREAM_FAILED:
		md_error("Stream failed...");
		break;
	}

	return;
}

static void md_context_drain_cb(pa_context* context, void* data) {

	pa_context_disconnect(context);

	return;
}

static void md_stream_drain_cb(pa_stream* stream, int success, void* data) {
	pa_operation* operation;

	operation = NULL;

	if (!success) {
		md_error("Failed to drain stream: %s",
			 pa_strerror(pa_context_errno(md_paudio.context)));
		return;
	}

	pa_stream_disconnect(stream);
	pa_stream_unref(stream);

	stream = NULL;

	operation = pa_context_drain(md_paudio.context, md_context_drain_cb,
				     NULL);
	if (!operation) {
		pa_context_disconnect(md_paudio.context);

		return;
	}

	pa_operation_unref(operation);

	return;
}

static void md_pa_drain(pa_stream* stream) {
	pa_operation* operation;

	operation = NULL;

	if (!stream)
		return;

	pa_stream_set_write_callback(stream, NULL, NULL);

	operation = pa_stream_drain(stream, md_stream_drain_cb, NULL);
	if (!operation) {
		md_error("Failed to drain stream: %s",
			 pa_strerror(pa_context_errno(md_paudio.context)));
		return;
	}

	pa_operation_unref(operation);

	return;
}

static void md_pa_stream_write_cb(pa_stream* stream,
				  size_t buf_size, void* data) {
	md_buf_pack_t* pack;
	int ret, buf_num;

	if (buf_size < get_settings()->buf_size)
		return;

	buf_num = (int)buf_size / get_settings()->buf_size;

	ret = md_buf_get_pack(&pack, &buf_num, MD_PACK_EXACT);
	switch (ret) {
	case MD_BUF_EXIT:
	case MD_BUF_NO_DECODERS:
		md_pa_drain(stream);
		return;
	default:
		break;
	}

	md_paudio_add_to_buffer(pack, stream);

	return;
}

static void md_pa_stream_suspended_cb(pa_stream* stream, void* data) {
	md_log("Stream suspended.");
	return;
}

static void md_pa_stream_moved_cb(pa_stream* stream, void* data) {
	md_log("Stream moved");
	return;
}

static void md_pa_stream_underflow_cb(pa_stream* stream, void* data) {
	md_log("Underflow");
	return;
}

static void md_pa_stream_overflow_cb(pa_stream* stream, void* data) {
	md_log("Overflow");
	return;
}

static void md_pa_stream_started_cb(pa_stream* stream, void* data) {
//	md_log("Stream started.");
	return;
}

static void md_pa_stream_event_cb(pa_stream* stream, const char* name,
				  pa_proplist* proplist, void* data) {
	md_log("Got event.");
	return;
}

static void md_pa_stream_buffer_attr_cb(pa_stream* stream, void* data) {
	md_log("Buffer attribute set.");
	return;
}

static void md_pa_stream_success_cb(pa_stream* stream, int success,
				    void* data) {
	md_log("Stream success!");
	return;
}

static void md_pa_new_stream(pa_context* context, void* data) {

	md_pa_get_sample_spec(md_buf_get_head()->metadata,
			      &md_paudio.sample_spec);

	md_log("New stream.");

	md_paudio.stream = pa_stream_new(context, "Melodeer",
					 &md_paudio.sample_spec,
					 NULL);
	if (!md_paudio.stream) {
		md_error("Failed to create stream: %s.",
			 pa_strerror(pa_context_errno(context)));
		return;
	}

	pa_stream_set_state_callback(md_paudio.stream,
				     md_pa_stream_state_cb, NULL);
	pa_stream_set_write_callback(md_paudio.stream,
				     md_pa_stream_write_cb, NULL);
	pa_stream_set_suspended_callback(md_paudio.stream,
					 md_pa_stream_suspended_cb, NULL);
	pa_stream_set_moved_callback(md_paudio.stream,
				     md_pa_stream_moved_cb, NULL);
	pa_stream_set_underflow_callback(md_paudio.stream,
					 md_pa_stream_underflow_cb, NULL);
	pa_stream_set_overflow_callback(md_paudio.stream,
					md_pa_stream_overflow_cb, NULL);
	pa_stream_set_started_callback(md_paudio.stream,
				       md_pa_stream_started_cb, NULL);
	pa_stream_set_event_callback(md_paudio.stream,
				     md_pa_stream_event_cb, NULL);
	pa_stream_set_buffer_attr_callback(md_paudio.stream,
					   md_pa_stream_buffer_attr_cb, NULL);

	md_log("Connecting...");

	if (pa_stream_connect_playback(md_paudio.stream, NULL,
				       &md_paudio.buffer_attr,
				       md_paudio.stream_flags,
				       NULL, NULL) < 0) {
		md_error("Failed to connect playback: %s",
			 pa_strerror(pa_context_errno(context)));
		return;
	}
	else {
		pa_stream_trigger(md_paudio.stream,
				  md_pa_stream_success_cb, NULL);
	}

	return;
}

void md_pa_set_state_cb(pa_context* context, void* data) {
	pa_context_state_t state;

	md_log("State changed.");

	state = pa_context_get_state(context);

	switch (state) {
	case PA_CONTEXT_TERMINATED:
		md_error("Terminated context.");
		break;

	case PA_CONTEXT_READY:
		md_log("READY");
		md_pa_new_stream(context, data);
		break;
	}

	return;
}

int md_paudio_deinit(void) {

	md_log("Pulseaudio deinitialized.");

	pthread_join(md_paudio.paudio_thread, NULL);

	return 0;
}

static void* md_paudio_mainloop_handler(void* data) {
	int ret;

	if (pa_mainloop_run(md_paudio.mainloop, &ret) < 0) {
		md_error("Could not run main loop.");
		pthread_exit(NULL);
	}

	return NULL;
}

int md_paudio_init(void) {

	md_paudio.mainloop = pa_mainloop_new();
	md_paudio.mainloop_api = pa_mainloop_get_api(md_paudio.mainloop);
	md_paudio.context = pa_context_new(md_paudio.mainloop_api, "melodeer");

	memset(&md_paudio.buffer_attr, 0, sizeof(md_paudio.buffer_attr));
	md_paudio.buffer_attr.maxlength = (uint32_t)-1;
	md_paudio.buffer_attr.prebuf = (uint32_t)get_settings()->buf_size
				     * get_settings()->buf_num;
	md_paudio.buffer_attr.minreq = (uint32_t)get_settings()->buf_size;

	pa_context_connect(md_paudio.context, NULL, 0, NULL);

	pa_context_set_state_callback(md_paudio.context,
				      md_pa_set_state_cb, NULL);

	if (pthread_create(&md_paudio.paudio_thread, NULL,
			   md_paudio_mainloop_handler, NULL)) {
		md_error("Could not create main loop thread.");

		return -EINVAL;
	}


	md_log("Initialized paudio driver.");

	return 0;
}

int md_paudio_play(void) {

	md_log("Dummy driver got play command.");

	return 0;
}

int md_paudio_stop(void) {

	md_log("Dummy driver got stop command.");

	return 0;
}

md_driver_t paudio_driver = {
	.name = "pulseaudio",
	.lib = "libpulse.so",
	.handle = NULL,
	.ops = {
		.load_symbols = md_paudio_load_symbols,
		.init = md_paudio_init,
		.play = md_paudio_play,
		.stop = md_paudio_stop,
		.pause = NULL,
		.get_state = NULL,
		.deinit = md_paudio_deinit
	}
};

register_driver(paudio, paudio_driver);
