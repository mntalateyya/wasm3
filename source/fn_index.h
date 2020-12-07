#ifndef fn_index_h
#define fn_index_h

#include "m3_env.h"

void build_fn_index(IM3Runtime runtime);

void find_fn(pc_t pc, u64 *fn_id, u64 *offset);

#endif