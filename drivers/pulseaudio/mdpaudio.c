#include "mddriver.h"

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>		/* memset */
#include <pulse/pulseaudio.h>
#include <pthread.h>

#include "mdmetadata.h"
#include "mdbuf.h"
#include "mdlog.h"

#define md_pa_error(CONTEXT, FMT, ...) do {		\
	md_error(FMT, ##__VA_ARGS__);			\
	if (CONTEXT)					\
		md_error("Pulseaudio: %s", pa_strerror(	\
			 pa_context_errno(CONTEXT)));	\
	md_driver_error_event();			\
} while (0)

md_driver_t paudio_driver;
static void md_pa_new_stream(pa_context* context, void* data);

#define PAUDIO_SYMBOLS(FUN)				\
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
	FUN(pa_stream_cork);				\
	FUN(pa_operation_get_state);			\
	FUN(pa_operation_unref);			\
	FUN(pa_stream_disconnect);			\
	FUN(pa_context_disconnect);			\
	FUN(pa_context_drain);				\
	FUN(pa_stream_unref);				\
	FUN(pa_threaded_mainloop_new);			\
	FUN(pa_threaded_mainloop_start);		\
	FUN(pa_threaded_mainloop_stop);			\
	FUN(pa_threaded_mainloop_get_api);		\
	FUN(pa_threaded_mainloop_wait);			\
	FUN(pa_threaded_mainloop_signal);		\
	FUN(pa_threaded_mainloop_lock);			\
	FUN(pa_threaded_mainloop_unlock);		\
	FUN(pa_threaded_mainloop_accept);		\
	FUN(pa_threaded_mainloop_free);

PAUDIO_SYMBOLS(md_define_fptr);

#define md_paudio_load_sym(_func) \
	md_load_sym(_func, &(paudio_driver), &(error))

int md_paudio_load_symbols(void) {
	int error;

	if (!paudio_driver.handle) {
		md_error("Pulseaudio library hasn't been opened.");

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
#define pa_stream_cork pa_stream_cork_ptr
#define pa_operation_get_state pa_operation_get_state_ptr
#define pa_operation_unref pa_operation_unref_ptr
#define pa_stream_disconnect pa_stream_disconnect_ptr
#define pa_context_disconnect pa_context_disconnect_ptr
#define pa_context_drain pa_context_drain_ptr
#define pa_stream_unref pa_stream_unref_ptr
#define pa_threaded_mainloop_new pa_threaded_mainloop_new_ptr
#define pa_threaded_mainloop_start pa_threaded_mainloop_start_ptr
#define pa_threaded_mainloop_stop pa_threaded_mainloop_stop_ptr
#define pa_threaded_mainloop_get_api pa_threaded_mainloop_get_api_ptr
#define pa_threaded_mainloop_wait pa_threaded_mainloop_wait_ptr
#define pa_threaded_mainloop_signal pa_threaded_mainloop_signal_ptr
#define pa_threaded_mainloop_lock pa_threaded_mainloop_lock_ptr
#define pa_threaded_mainloop_unlock pa_threaded_mainloop_unlock_ptr
#define pa_threaded_mainloop_accept pa_threaded_mainloop_accept_ptr
#define pa_threaded_mainloop_free pa_threaded_mainloop_free_ptr

typedef enum {
	MD_PA_STOPPED,
	MD_PA_RUNNING,
	MD_PA_EXIT
} md_pa_state_t;

static struct md_paudio_t {
	pa_threaded_mainloop* mainloop;
	pa_mainloop_api* mainloop_api;
	pa_context* context;
	pa_operation* operation;
	pa_sample_spec sample_spec;
	pa_buffer_attr buffer_attr;
	pa_stream_flags_t stream_flags;
	pa_stream* stream;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	md_pa_state_t state;
} md_paudio;

static bool md_pa_suspend(void) {

	pthread_mutex_lock(&md_paudio.mutex);
	if (md_paudio.state == MD_PA_EXIT) {
		pthread_mutex_unlock(&md_paudio.mutex);
		return false;
	}
	md_paudio.state = MD_PA_STOPPED;
	while (md_paudio.state == MD_PA_STOPPED)
		pthread_cond_wait(&md_paudio.cond, &md_paudio.mutex);
	pthread_mutex_unlock(&md_paudio.mutex);

	return true;
}

static void md_pa_resume(void) {

	pthread_mutex_lock(&md_paudio.mutex);
	if (md_paudio.state == MD_PA_EXIT) {
		pthread_mutex_unlock(&md_paudio.mutex);
		return;
	}
	md_paudio.state = MD_PA_RUNNING;
	pthread_cond_signal(&md_paudio.cond);
	pthread_mutex_unlock(&md_paudio.mutex);

	return;
}

static void md_pa_exit(void) {

	pthread_mutex_lock(&md_paudio.mutex);
	md_paudio.state = MD_PA_EXIT;
	pthread_cond_signal(&md_paudio.cond);
	pthread_mutex_unlock(&md_paudio.mutex);

	return;
}

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

		for (int j = 0; j < curr_chunk->size; j++)
			curr_chunk->chunk[j] = 0;

		if (pa_stream_write(stream, curr_chunk->chunk,
				    curr_chunk->size, NULL,
				    0, PA_SEEK_RELATIVE) < 0) {
			md_pa_error(md_paudio.context,
				    "Could not write to stream.");
			return -EINVAL;
		}

		i++;
		curr_chunk = buf_pack->next(buf_pack);
	}

	md_buf_clean_pack(buf_pack);

	return 0;
}

int md_paudio_deinit(void) {

	md_pa_exit();

	pa_threaded_mainloop_lock(md_paudio.mainloop);
	while (md_paudio.context)
		pa_threaded_mainloop_wait(md_paudio.mainloop);
	pa_threaded_mainloop_unlock(md_paudio.mainloop);

	md_log("Context disconnected properly.");

	pa_threaded_mainloop_stop(md_paudio.mainloop);
	pa_threaded_mainloop_free(md_paudio.mainloop);

	md_log("Pulseaudio deinitialized.");

	return 0;
}

static void md_context_drain_cb(pa_context* context, void* data) {

	md_log("Drained context.");

	md_driver_signal_state(MD_DRIVER_STOPPED);

	while (!md_buf_get_head()) {

		if (!md_pa_suspend()) {
			pa_context_disconnect(context);
			return;
		}
	}

	md_pa_new_stream(context, data);

	return;
}

static void md_pa_context_drain(pa_context* context) {
	pa_operation* operation;

	operation = pa_context_drain(md_paudio.context,
				     md_context_drain_cb, NULL);
	if (!operation) {
		md_pa_error(NULL, "No operation from context drain.");
		pa_context_disconnect(md_paudio.context);

		return;
	}

	pa_operation_unref(operation);

	return;
}

static void md_stream_drain_cb(pa_stream* stream, int success, void* data) {

	md_log("Drained stream.");

	if (!success) {
		md_pa_error(md_paudio.context, "Failed to drain stream.");

		return;
	}

	pa_stream_disconnect(stream);
	pa_stream_unref(stream);

	stream = NULL;

	md_pa_context_drain(md_paudio.context);

	return;
}

static void md_pa_drain(pa_stream* stream) {
	pa_operation* operation;

	if (!stream) {
		md_log("No stream.");
		md_pa_context_drain(md_paudio.context);
		return;
	}

	pa_stream_set_write_callback(stream, NULL, NULL);

	operation = pa_stream_drain(stream, md_stream_drain_cb, NULL);
	if (!operation) {
		md_pa_error(md_paudio.context,
			    "No operation from stream drain.");

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
	if (!ret || (ret == MD_BUF_NO_DECODERS))
		md_paudio_add_to_buffer(pack, stream);

	if ((ret == MD_BUF_EXIT) || (ret == MD_BUF_NO_DECODERS))
		md_pa_drain(stream);

	return;
}

static void md_pa_stream_state_cb(pa_stream* stream, void* data) {

	switch (pa_stream_get_state(stream)) {
	case PA_STREAM_CREATING:
		md_log("Creating stream...");
		break;
	case PA_STREAM_TERMINATED:
		md_log("Terminated stream...");
		break;
	case PA_STREAM_READY:
		md_log("Stream ready...");
		break;
	case PA_STREAM_FAILED:
		md_pa_error(NULL, "Stream failed...");
		break;
	default:
		md_log("Stream state changed.");
		break;
	}

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

	md_driver_buffer_underrun_event(false);

	return;
}

static void md_pa_stream_overflow_cb(pa_stream* stream, void* data) {

	md_log("Overflow");

	return;
}

static void md_pa_stream_started_cb(pa_stream* stream, void* data) {

	md_driver_signal_state(MD_DRIVER_PLAYING);

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
	md_buf_chunk_t* head;

	do {
		head = md_buf_get_head();
		if (!head) {
			if (!md_pa_suspend()) {
				pa_context_disconnect(context);
				return;
			}
		}
	} while (!head);

	md_pa_get_sample_spec(head->metadata, &md_paudio.sample_spec);

	md_paudio.stream = pa_stream_new(context, "Playback Stream",
					 &md_paudio.sample_spec,
					 NULL);
	if (!md_paudio.stream) {
		md_pa_error(context, "Failed to create stream.");

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

	if (pa_stream_connect_playback(md_paudio.stream, NULL,
				       &md_paudio.buffer_attr,
				       md_paudio.stream_flags,
				       NULL, NULL) < 0) {
		md_pa_error(context, "Failed to connect playback.");
		return;
	}
	else {
		pa_stream_trigger(md_paudio.stream,
				  md_pa_stream_success_cb, NULL);
	}

	return;
}

static void md_pa_set_state_cb(pa_context* context, void* data);

static void md_pa_setup_context(void) {

	md_log("Setting up context...");

	pa_context_connect(md_paudio.context, NULL, 0, NULL);

	pa_context_set_state_callback(md_paudio.context,
				      md_pa_set_state_cb, NULL);

	return;
}

static void md_pa_set_state_cb(pa_context* context, void* data) {
	pa_context_state_t state;

	state = pa_context_get_state(context);

	switch (state) {
	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		md_log("Terminated context.");
		md_paudio.context = NULL;
		pa_threaded_mainloop_signal(md_paudio.mainloop, 0);
		break;

	case PA_CONTEXT_READY:
		md_log("Context ready!");
		md_pa_new_stream(context, data);
		break;
	}

	return;
}

int md_paudio_init(void) {
	int buf_size, buf_num;

	buf_size = get_settings()->buf_size;
	buf_num = get_settings()->buf_num;

	md_paudio.mainloop = pa_threaded_mainloop_new();
	md_paudio.mainloop_api = pa_threaded_mainloop_get_api(
					md_paudio.mainloop);
	md_paudio.context = pa_context_new(md_paudio.mainloop_api,
					   "Melodeer");

	memset(&md_paudio.buffer_attr, 0, sizeof(md_paudio.buffer_attr));
	md_paudio.buffer_attr.maxlength = (uint32_t)-1;
	md_paudio.buffer_attr.prebuf = (uint32_t)buf_size * buf_num;
	md_paudio.buffer_attr.minreq = (uint32_t)buf_size;

	md_pa_setup_context();

	pthread_mutex_init(&md_paudio.mutex, NULL);
	pthread_cond_init(&md_paudio.cond, NULL);
	md_paudio.state = MD_PA_RUNNING;

	if (pa_threaded_mainloop_start(md_paudio.mainloop) < 0) {
		md_pa_error(NULL, "Could not run main loop.");

		return -EINVAL;
	}

	md_log("Initialized Pulseaudio driver.");

	return 0;
}

md_driver_state_ret_t md_paudio_stop(void) {

	/* As soon as packages from buffer are
	 * drained, Pulseaudio will stop and
	 * inform generic driver layer through
	 * callback of change state. */

	return MD_DRIVER_STATE_WILL_BE_SET;
}

md_driver_state_ret_t md_paudio_resume(void) {

	md_pa_resume();

	return MD_DRIVER_STATE_WILL_BE_SET;
}

typedef void(*cork_cb)(pa_stream*, int, void*);

static md_driver_state_ret_t md_paudio_cork(int state, cork_cb cb) {
	pa_operation* op;

	pa_threaded_mainloop_lock(md_paudio.mainloop);
	op = pa_stream_cork(md_paudio.stream, 0, cb, NULL);
	while (pa_operation_get_state(op) != PA_OPERATION_DONE)
		pa_threaded_mainloop_wait(md_paudio.mainloop);
	pa_threaded_mainloop_unlock(md_paudio.mainloop);

	pa_operation_unref(op);

	return MD_DRIVER_STATE_WILL_BE_SET;
}

static void md_paudio_pause_success_cb(pa_stream* stream,
				       int success, void* data) {

	if (success)
		md_driver_signal_state(MD_DRIVER_PAUSED);
	else
		md_pa_error(NULL, "Could not pause stream.");

	return;
}

md_driver_state_ret_t md_paudio_pause(void) {

	return md_paudio_cork(0, md_paudio_pause_success_cb);
}

static void md_paudio_unpause_success_cb(pa_stream* stream,
					 int success, void* data) {

	if (success)
		md_driver_signal_state(MD_DRIVER_PLAYING);
	else
		md_pa_error(NULL, "Could not unpause stream.");

	return;
}

md_driver_state_ret_t md_paudio_unpause(void) {

	return md_paudio_cork(1, md_paudio_unpause_success_cb);
}

md_driver_t paudio_driver = {
	.name = "pulseaudio",
	.lib = "libpulse.so",
	.handle = NULL,
	.ops = {
		.load_symbols = md_paudio_load_symbols,
		.init = md_paudio_init,
		.resume = md_paudio_resume,
		.stop = md_paudio_stop,
		.pause = md_paudio_pause,
		.unpause = md_paudio_unpause,
		.deinit = md_paudio_deinit
	}
};

register_driver(paudio, paudio_driver);
