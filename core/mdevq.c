#include "mdevq.h"

#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "mdll.h"
#include "mdlog.h"

static bool evq_glob_run;
static int evq_glob_error;

int md_evq_init(md_evq_t* evq) {

	__ll_init(evq->ll_take_in);
	__ll_init(evq->ll_take_out);

	return 0;
}

int md_evq_add_event(md_evq_t* evq, md_event_type_t evtype,
		     md_event_cb cb, void* data) {
	md_evqll_t** curr_evq;
	md_evqll_t* new_ev;
	md_evqll_t* temp;

	switch (evtype) {
	case MD_EVENT_RUN_ON_TAKE_IN:
		curr_evq = &(evq->ll_take_in);
		break;

	case MD_EVENT_RUN_ON_TAKE_OUT:
		curr_evq = &(evq->ll_take_out);
		break;

	default:
		return -EINVAL;
	}

	new_ev = malloc(sizeof(*new_ev));
	if (!new_ev) {
		md_error("Could not allocate memory.");
		return -ENOMEM;
	}

	new_ev->event.callback = cb;
	new_ev->event.data = data;
	new_ev->next = NULL;

	__ll_add(*curr_evq, new_ev, temp);

	return 0;
}

static void md_evq_free(md_evqll_t* curr) {
	md_evqll_t* temp;
	int error;

	if (evq_glob_run) {
		error = curr->event.callback(curr->event.data);
		if (error && !evq_glob_error)
			evq_glob_error = error;
	}

	temp = curr;
	free(temp);

	return;
}

int md_evq_run_queue(md_evq_t* evq, md_event_type_t type) {
	md_evqll_t* curr;
	md_evqll_t* next;

	evq_glob_run = true;
	evq_glob_error = 0;

	switch (type) {
	case MD_EVENT_RUN_ON_TAKE_IN:
		__ll_deinit(evq->ll_take_in, md_evq_free, curr, next);
		evq->ll_take_in = NULL;

		break;
	case MD_EVENT_RUN_ON_TAKE_OUT:
		__ll_deinit(evq->ll_take_out, md_evq_free, curr, next);
		evq->ll_take_in = NULL;

		break;
	default:
		return -EINVAL;
	}


	return evq_glob_error;
}

int md_evq_deinit(md_evq_t* evq) {
	md_evqll_t* curr;
	md_evqll_t* next;

	evq_glob_run = false;
	evq_glob_error = 0;

	__ll_deinit(evq->ll_take_in, md_evq_free, curr, next);
	evq->ll_take_in = NULL;

	__ll_deinit(evq->ll_take_out, md_evq_free, curr, next);
	evq->ll_take_out = NULL;

	return evq_glob_error;
}

