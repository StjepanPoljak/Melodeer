#include "mddriver.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "mdlog.h"
#include "mdll.h"
#include "mdsettings.h"
#include "mdcoreops.h"
#include "mdgarbage.h"

static md_driver_ll* md_driverll_head;
static int underrun_count = 0;
static int prev_underrun_count = 0;

#define md_driver_set_state(COND_SAME, OPERATION,		\
			    TARGET_STATE) ({			\
	md_driver_state_ret_t ret;				\
	pthread_mutex_lock(&md_driver.mutex);			\
	if (COND_SAME)						\
		ret = MD_DRIVER_STATE_SAME;			\
	else if (md_driver.state != TARGET_STATE)		\
		ret = MD_DRIVER_STATE_NOT_SET;			\
	else {							\
		ret = OPERATION();				\
		if (ret == MD_DRIVER_STATE_SET)			\
			md_driver.state = TARGET_STATE;		\
	}							\
	pthread_mutex_unlock(&md_driver.mutex), ret;		\
})

#define curr_driver_ops() get_settings()->driver->ops

static struct md_driver_data_t {
	pthread_mutex_t mutex;
	md_driver_state_t state;
} md_driver;

DEFINE_SYM_FUNCTIONS(driver);

md_driver_ll* md_driver_ll_add(md_driver_t* driver) {
	md_driver_ll* last;
	md_driver_ll* new;

	new = malloc(sizeof(*new));
	if (!new) {
		md_error("Could not allocate memory.");
		return NULL;
	}

	new->driver = driver;
	new->next = NULL;

	__ll_add(md_driverll_head, new, last);

	return new;
}

md_driver_t* md_driver_ll_find(const char* name) {
	md_driver_ll* curr;

	__ll_find(md_driverll_head, name, driver, curr);

	return curr ? curr->driver : NULL;
}

static void md_driver_ll_free(md_driver_ll* curr) {
	md_driver_ll* temp;

	temp = curr;
	free(temp);

	return;
}

int md_driver_ll_deinit(void) {
	md_driver_ll* curr;
	md_driver_ll* next;

	__ll_deinit(md_driverll_head,
		    md_driver_ll_free,
		    curr, next);

	return 0;
}

int md_driver_init(void) {

	pthread_mutex_init(&md_driver.mutex, NULL);

	md_driver.state = MD_DRIVER_STOPPED;

	return curr_driver_ops().init();
}

static void md_driver_stopped_event(void) {

	md_garbage_clean();
	md_exec_event(stopped);

	return;
}

static void md_driver_playing_event(void) {

	underrun_count = 0;
	md_exec_event(playing);

	return;
}

static void md_driver_paused_event(void) {

	md_exec_event(paused);

	return;
}

void md_driver_signal_state(md_driver_state_t state) {

	pthread_mutex_lock(&md_driver.mutex);
	if (md_driver.state == state) {
		pthread_mutex_unlock(&md_driver.mutex);
		return;
	}
	md_driver.state = state;
	pthread_mutex_unlock(&md_driver.mutex);

	switch (state) {
	case MD_DRIVER_STOPPED:
		md_driver_stopped_event();
		break;

	case MD_DRIVER_PLAYING:
		md_driver_playing_event();
		break;

	case MD_DRIVER_PAUSED:
		md_driver_paused_event();
		break;

	default:
		break;
	}

	return;
}

md_driver_state_ret_t md_driver_pause(void) {
	md_driver_state_ret_t ret;

	ret = md_driver_set_state(md_driver.state == MD_DRIVER_PLAYING,
				  curr_driver_ops().pause,
				  MD_DRIVER_PAUSED);

	if (ret == MD_DRIVER_STATE_SET)
		md_driver_paused_event();

	return ret;
}

md_driver_state_ret_t md_driver_unpause(void) {
	md_driver_state_ret_t ret;

	ret = md_driver_set_state(md_driver.state == MD_DRIVER_PAUSED,
				  curr_driver_ops().unpause,
				  MD_DRIVER_PLAYING);

	if (ret == MD_DRIVER_STATE_SET)
		md_driver_playing_event();

	return ret;
}

md_driver_state_ret_t md_driver_stop(void) {
	md_driver_state_ret_t ret;

	ret = md_driver_set_state(md_driver.state == MD_DRIVER_PLAYING ||
				  md_driver.state == MD_DRIVER_PAUSED,
				  curr_driver_ops().stop,
				  MD_DRIVER_PLAYING);

	if (ret == MD_DRIVER_STATE_SET)
		md_driver_stopped_event();

	return ret;
}

md_driver_state_ret_t md_driver_resume(void) {
	md_driver_state_ret_t ret;

	ret = md_driver_set_state(md_driver.state == MD_DRIVER_STOPPED,
				  curr_driver_ops().resume,
				  MD_DRIVER_PLAYING);

	if (ret == MD_DRIVER_STATE_SET)
		md_driver_playing_event();

	return ret;
}

void md_driver_buffer_underrun_event(bool critical) {
	static time_t last_measure = 0;
	static float expected_avg = 0;
	time_t curr_time;
	time_t elapsed;
	float avg;

	expected_avg = expected_avg + ((expected_avg == 0)
		    * (get_settings()->buf_underrun.count
		    / (float)get_settings()->buf_underrun.secs));

	underrun_count++;

	if (critical) {
		md_exec_event(buffer_underrun, true);

		return;
	}

	curr_time = time(NULL);
	elapsed = curr_time - last_measure;

	if (elapsed >= get_settings()->buf_underrun.secs) {
		avg = (float)underrun_count + prev_underrun_count;
		avg = avg / (float)(get_settings()->buf_underrun.secs * 2);

		if (underrun_count > get_settings()->buf_underrun.count
		 || avg >= expected_avg) {
			md_exec_event(buffer_underrun, false);
			prev_underrun_count = underrun_count;
			underrun_count = 0;
		}

		last_measure = curr_time;
	}

	return;
}

void md_driver_error_event(void) {

	md_exec_event(driver_error);

	return;
}
