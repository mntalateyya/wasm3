#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "m3.h"
#include "m3_env.h"
#include "m3_exec_defs.h"
#include "m3_exec.h"

volatile bool m3_migration_flag = false;
static IM3Runtime runtime;

void *migration_thread(FILE *input) {
	usleep(100);
	m3_migration_flag = true;
	printf("[migration thread woke up]\n");
	while(1)
		usleep(10000000);
	return NULL;
}

void m3_migration_init(IM3Runtime _runtime) {
	pthread_t tid;
	pthread_create(&tid, NULL, migration_thread, stdin);

	runtime = _runtime;
}

M3Result m3_dump_state(d_m3OpSig) {
	printf( "_pc: %p\n"
	        "_sp: %p\n"
	        "_mem: %p\n"
			"_r0: %ld\n"
			"_fp0: %lf\n"
			"stack: %p\n",
			d_m3OpAllArgs, runtime->stack);
	IM3Function func;
    m3_FindFunction (&func, runtime, "_start");
	FILE *out = fopen("wasm3dump", "w");
	_pc = (u64)_pc - (u64)func->compiled;
	fwrite(&_pc, sizeof(_pc), 1, out);
	_sp = (u64)_sp - (u64)runtime->stack;
	fwrite(&_sp, sizeof(_sp), 1, out);
	fwrite(&_mem, sizeof(_mem), 1, out);
	fwrite(&_r0, sizeof(_r0), 1, out);
	fwrite(&_fp0, sizeof(_fp0), 1, out);
	fwrite(&runtime->numStackSlots, sizeof(runtime->numStackSlots), 1, out);
	fwrite(runtime->stack, sizeof(m3reg_t), runtime->numStackSlots, out);
	fclose(out);
	exit(0);
}

M3Result m3_resume_state(IM3Runtime _runtime) {
	M3Result result = c_m3Err_none;

	IM3Function func;
    result = m3_FindFunction (&func, _runtime, "_start");
    if (result) return result;

	pc_t _pc;
	u64 *_sp;
	M3MemoryHeader *_mem;
	m3reg_t _r0;
	f64 _fp0;

	FILE *f = fopen("wasm3dump", "r");
	u32 stack_size;
	fread(&_pc, sizeof(_pc), 1, f);
	_pc = (u64)_pc + (u64)func->compiled;
	fread(&_sp, sizeof(_sp), 1, f);
	_sp = (u64)_sp + (u64)_runtime->stack;
	fread(&_mem, sizeof(_mem), 1, f);
	fread(&_r0, sizeof(_r0), 1, f);
	fread(&_fp0, sizeof(_fp0), 1, f);
	fread(&stack_size, sizeof(stack_size), 1, f);
	assert(_runtime->numStackSlots == stack_size);
	fread(_runtime->stack, sizeof(m3reg_t), _runtime->numStackSlots, f);

	printf( "_pc: %p\n"
	        "_sp: %p\n"
	        "_mem: %p\n"
			"_r0: %ld\n"
			"_fp0: %lf\n"
			"stack: %p\n",
			d_m3OpAllArgs, _runtime->stack);
	
	nextOp();
	u64 * stack = _runtime->stack;
	printf("[%ld, %ld, ...]\n", stack[0], stack[1]);
	exit(0);
	return NULL;
}