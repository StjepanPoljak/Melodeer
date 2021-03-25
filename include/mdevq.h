#ifndef MD_EVQ_H
#define MD_EVQ_H

typedef struct {
	void*(*callback)(void*);
	void* data;
} md_event_t;

int md_evq_init(void);
int md_evq_loop(void);
int md_evq_send(md_event_t*);

#endif
