#ifndef MD_SYM_H
#define MD_SYM_H

#include <dlfcn.h>

#define md_load_sym(_func, _struct, _error) do {			\
	_func ##_ptr  = dlsym((_struct)->handle, #_func);		\
	if (dlerror()) {						\
		md_error("Error loading " #_func ".");			\
		*(_error) = -EINVAL;					\
	}								\
} while (0)

#define md_define_fptr(_var)						\
	typedef typeof(_var) _var ##_t;					\
	_var ##_t* _var ##_ptr;

#define DECLARE_SYM_FUNCTIONS(_struct)					\
int md_##_struct##_try_load(md_##_struct##_t*);				\
void md_##_struct##_unload(md_##_struct##_t*)

#define DEFINE_SYM_FUNCTIONS(_struct)					\
void md_##_struct##_unload(md_##_struct##_t* _struct) {			\
	if ((_struct)->handle) dlclose((_struct)->handle); return;	\
}									\
int md_##_struct##_try_load(md_##_struct##_t* _struct) {		\
	if (!(_struct)->lib) return 0;					\
	if (!(_struct)->handle) {					\
		(_struct)->handle = dlopen((_struct)->lib, RTLD_NOW);	\
		if (!(_struct)->handle) {				\
			md_error("Could not find library %s "		\
				 "for %s %s.", (_struct)->lib,		\
				 #_struct, (_struct)->name);		\
			return -EINVAL; } }				\
	if ((_struct)->ops.load_symbols()) {				\
		md_error("Could not load %s symbols.",			\
			(_struct)->name);				\
		md_##_struct##_unload((_struct));			\
		return -EINVAL;	}					\
	return 0;							\
}

#endif
