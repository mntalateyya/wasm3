#ifndef fn_index_h
#define fn_index_h

#include "m3_env.h"

void build_fn_index(IM3Module);

void find_fn_and_offset(IM3Module module, pc_t pc, u64 *fn_id, u64 *offset);

pc_t goto_fn_offset(IM3Module module, u64 fn_id, u64 offset);

#endif