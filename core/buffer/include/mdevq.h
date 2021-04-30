#ifndef MD_EVQ_H
#define MD_EVQ_H

typedef enum {
	MD_EVENT_RUN_ON_TAKE_IN,
	MD_EVENT_RUN_ON_TAKE_OUT
} md_event_type_t;

typedef int(*md_event_cb)(void*);

typedef struct {
	md_event_cb callback;
	void* data;
} md_event_t;

typedef struct md_evqll_t {
	md_event_t event;
	struct md_evqll_t* next;
} md_evqll_t;

typedef struct {
	md_evqll_t* ll_take_in;
	md_evqll_t* ll_take_out;
} md_evq_t;

int md_evq_init(md_evq_t*);
int md_evq_add_event(md_evq_t*, md_event_type_t, md_event_cb, void*);
int md_evq_run_queue(md_evq_t*, md_event_type_t);
int md_evq_deinit(md_evq_t*);

#endif
