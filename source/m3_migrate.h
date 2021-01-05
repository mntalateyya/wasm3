#ifndef m3_migrate_h
#define m3_migrate_h

//#include "m3.h"
#include "m3_exec_defs.h"

long long microtime(void);
m3ret_t jmp_start(d_m3OpSig);
void jump_return(d_m3OpSig);
void m3_migration_init(const char*);
bool m3_migration_flag();
M3Result m3_dump_state(d_m3OpSig);
M3Result m3_resume_state(IM3Runtime runtime);

#endif //m3_migrate_h