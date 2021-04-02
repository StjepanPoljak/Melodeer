#include "mdcoreops.h"

static md_core_ops_t* md_core_ops;

void md_set_core_ops(md_core_ops_t* core_ops) {

	md_core_ops = core_ops;

	return;
}

md_core_ops_t* md_get_core_ops(void) {

	return md_core_ops;
}
