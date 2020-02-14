#ifndef m3_migrate_h
#define m3_migrate_h

//#include "m3.h"
#include "m3_exec_defs.h"


void m3_migration_init();
bool m3_migration_flag();
M3Result m3_dump_state(d_m3OpSig);
M3Result m3_resume_state(IM3Runtime runtime);

#endif //m3_migrate_h