//
//  m3_exec_defs.h
//  m3
//
//  Created by Steven Massey on 5/1/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

#ifndef m3_exec_defs_h
#define m3_exec_defs_h

#include "m3_core.h"

typedef struct {
    pc_t pc;
    u64 sp;
    u64 r;
    f64 fp;
} cs_frame;

#   define d_m3OpSig                pc_t _pc, u64 * _sp, M3MemoryHeader * _mem, m3reg_t _r0, f64 _fp0, cs_frame * _cs
#   define d_m3OpArgs               _sp, _mem, _r0, _fp0, _cs
#   define d_m3OpAllArgs            _pc, _sp, _mem, _r0, _fp0, _cs
#   define d_m3OpDefaultArgs        666, NAN

#   define m3MemData(mem)           (u8*)((M3MemoryHeader*)(mem)+1)

typedef m3ret_t (vectorcall * IM3Operation) (d_m3OpSig);

#endif /* m3_exec_defs_h */
