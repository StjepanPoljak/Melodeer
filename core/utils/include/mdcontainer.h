#ifndef MD_CONTAINER_H
#define MD_CONTAINER_H

#define container_of(ptr, type, member) ({				\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);		\
	(type *)( (char *)__mptr - __builtin_offsetof(type, member) ); })

#endif
