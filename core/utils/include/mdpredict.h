#ifndef MD_PREDICT_H
#define MD_PREDICT_H

#define md_likely(x)	__builtin_expect(!!(x), 1)
#define md_unlikely(x)	__builtin_expect(!!(x), 0)

#endif
