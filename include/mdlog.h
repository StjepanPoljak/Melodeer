#ifndef MD_LOG_H
#define MD_LOG_H

#include <stdio.h>

#ifdef CONFIG_DEBUG

#include "mdutils.h"

#define __FILENAME__ md_basename_of(__FILE__)

#define md_log_raw(symbol, fmt, ...) \
do { \
	printf("(%c) [%s:%d]: " fmt "\n", symbol, \
	       __FILENAME__, __LINE__, ## __VA_ARGS__); \
} while (0)

#define md_trace(fmt, ...)

#else

#define md_log_raw(symbol, fmt, ...) \
do { \
	printf("(%c) " fmt "\n", symbol, ## __VA_ARGS__); \
} while (0)

#define md_trace(fmt, ...) \
do { \
	printf("\t-> " fmt "\n", ## __VA_ARGS__); \
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
