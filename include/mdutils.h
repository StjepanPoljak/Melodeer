#ifndef MD_UTILS_H
#define MD_UTILS_H

#include <string.h>

#define md_basename_of(file) ({					\
	const char* fname = strrchr(file, '/');			\
	fname ? fname + 1 : file;				\
})

#define md_extension_of(file) ({				\
	const char* dot = strrchr(md_basename_of(file), '.');	\
	dot ? dot + 1 : "";					\
})

#endif

