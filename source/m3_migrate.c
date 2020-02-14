#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "m3.h"
#include "m3_env.h"

static volatile bool migration_flag = false;

bool m3_migration_flag() {
	return migration_flag;
}

void *migration_clock(FILE *input) {
	usleep(100);
	migration_flag = true;
	printf("[migration thread woke up]\n");
	while(1)
		usleep(10000000);
	return NULL;
}

void m3_migration_init() {
	pthread_t tid;
	pthread_create(&tid, NULL, migration_clock, stdin);
}

M3Result m3_dump_state(d_m3OpSig) {
	IM3Runtime runtime = _mem->runtime;
	printf("[dumping state]\n");
	printf( "_pc: %p\n"
	        "_sp: %p\n"
	        "_mem: %p\n"
			"_r0: %ld\n"
			"_fp0: %lf\n"
			"stack: %p\n",
			d_m3OpAllArgs, runtime->stack);
	IM3Function func;
    m3_FindFunction (&func, runtime, "main");
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
	fwrite(&runtime->memory.numPages, sizeof(runtime->memory.numPages), 1, out);
	fwrite(runtime->memory.mallocated+1,
	       runtime->memory.numPages * c_m3MemPageSize, 1, out);
	fclose(out);
	exit(0);
}

// TODO: worry about allocations when loading memory and stack 
M3Result m3_resume_state(IM3Runtime runtime) {
	M3Result result = c_m3Err_none;

	IM3Function func;
    result = m3_FindFunction (&func, runtime, "main");
    if (result) return result;

	pc_t _pc;
	u64 *_sp;
	M3MemoryHeader *_mem, *ignore;
	m3reg_t _r0;
	f64 _fp0;

	FILE *f = fopen("wasm3dump", "r");

	// load basic registers
	fread(&_pc, sizeof(_pc), 1, f);
	_pc = (u64)_pc + (u64)func->compiled;
	fread(&_sp, sizeof(_sp), 1, f);
	_sp = (u64)_sp + (u64)runtime->stack;
	fread(&ignore, sizeof(ignore), 1, f);
	_mem = runtime->memory.mallocated;
	fread(&_r0, sizeof(_r0), 1, f);
	fread(&_fp0, sizeof(_fp0), 1, f);

	// load stack
	u32 stack_size;
	fread(&stack_size, sizeof(stack_size), 1, f);
	assert(runtime->numStackSlots == stack_size);
	fread(runtime->stack, sizeof(m3reg_t), runtime->numStackSlots, f);

	// load memory
	fread(&runtime->memory.numPages, sizeof(runtime->memory.numPages), 1, f);
	assert(runtime->memory.numPages <= runtime->memory.maxPages);
	fread(runtime->memory.mallocated+1,
	      runtime->memory.numPages * c_m3MemPageSize, 1, f);

	printf("[resumed state]\n");
	printf( "_pc: %p\n"
	        "_sp: %p\n"
	        "_mem: %p\n"
			"_r0: %ld\n"
			"_fp0: %lf\n"
			"stack: %p\n",
			d_m3OpAllArgs, runtime->stack);
	
	nextOp();
	u64 * stack = runtime->stack;
	printf("[%ld, %ld, ...]\n", stack[0], stack[1]);
	exit(0);
	return NULL;
}