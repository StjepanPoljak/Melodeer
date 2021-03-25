#ifndef MD_LL_H
#define MD_LL_H

#define __ll_init(_head)				\
do {							\
	(_head) = NULL;					\
} while (0)

#define __ll_add(_head, _new, _temp) 			\
do {							\
	if (!(_head))					\
		(_head) = (_new);			\
	else {						\
		(_temp) = (_head);			\
		while ((_temp)->next)			\
			(_temp) = (_temp)->next;	\
		(_temp)->next = (_new);			\
	}						\
} while (0)

#define __ll_find(_head, _name, _temp)			\
do {							\
	(_temp) = (_head);				\
	while ((_temp)					\
		&& strcmp((_name),			\
			  (_temp)->driver->name))	\
		(_temp) = (_temp)->next;		\
} while (0)

#define __ll_deinit(_head, _free_f, _curr, _next) 	\
do {							\
	(_curr) = (_head);				\
	while ((_curr)) {				\
		(_next) = (_curr)->next;		\
		(_free_f)(_curr);			\
		(_curr) = (_next);			\
	}						\
} while (0)

#endif
