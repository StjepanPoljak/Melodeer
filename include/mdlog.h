#ifndef MD_LOG_H
#define MD_LOG_H

#include <stdio.h>

#ifdef CONFIG_DEBUG

#include <string.h>

#define __FILENAME__ ({ \
	char* fname = strrchr(__FILE__, '/'); \
	fname ? fname + 1 : __FILE__; \
})	

#define md_log_raw(symbol, fmt, ...) \
do { \
	printf("(%c) [%s:%d]: %s\n", symbol, \
	       __FILENAME__, __LINE__, \
	       fmt, ## __VA_ARGS__); \
} while (0)

#define md_trace(fmt, ...)

#else

#define md_log_raw(symbol, fmt, ...) \
do { \
	printf("(%c) %s\n", symbol, fmt, ## __VA_ARGS__); \
} while (0)

#define md_trace(fmt, ...) \
do { \
	printf("\t-> %s\n", fmt, ## __VA_ARGS__); \
} while (0)

#endif

#define md_log(fmt, ...) \
do { \
	 md_log_raw('i', fmt, ## __VA_ARGS__); \
} while (0)

#define md_warning(fmt, ...) \
do { \
	md_log_raw('*', fmt, ## __VA_ARGS__); \
while (0)

#define md_error(fmt, ...) \
do { \
	md_log_raw('!', fmt, ## __VA_ARGS__); \
} while (0)

#endif
