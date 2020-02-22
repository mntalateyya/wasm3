#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <setjmp.h>

#include "m3.h"
#include "m3_env.h"

static volatile bool migration_flag = false;
static jmp_buf jmp_env;
static struct FnIndex {
	void *ptr;
	u32 idx;
} *fn_index;
static size_t fn_count;

bool m3_migration_flag() {
	return migration_flag;
}

void jump_return(d_m3OpSig) {
	longjmp(jmp_env, 1);
}

void jmp(d_m3OpSig) {
	if (setjmp(jmp_env) == 0) {
		Call(d_m3OpAllArgs);
	}
}

void jmp_start(d_m3OpSig) {
	IM3Runtime runtime = _mem->runtime;
	void *ptr = jump_return;
	runtime->callStack[0].pc = &ptr;
	if (_cs == runtime->callStack)
		_cs++;
	jmp(d_m3OpAllArgs);
}

int fidx_cmp(const void *ptr1, const void *ptr2) {
	const struct FnIndex *f1 = ptr1;
	const struct FnIndex *f2 = ptr2;
	return (size_t)f1->ptr - (size_t)f2->ptr;
}

// works for single module only
void build_fn_index(IM3Runtime runtime) {
	fn_count = runtime->modules->numFunctions;
	fn_index = malloc(sizeof(*fn_index) * fn_count);
	for (size_t i = 0; i < fn_count; i++) {
		fn_index[i].ptr = runtime->modules->functions[i].compiled;
		fn_index[i].idx = i;
	}
	
	qsort(fn_index, fn_count, sizeof(*fn_index), fidx_cmp);
}

void find_fn(pc_t pc, u32 *fn_id, u32 *offset) {
	struct FnIndex *fn = &fn_index[0];
	i32 lo = 0, hi = fn_count;
	while (lo < hi) {
		i32 mid = lo + (hi - lo)/2;
		int cmp = fidx_cmp(&fn_index[mid], &pc);
		if (cmp == 0) {
			fn = &fn_index[mid];
			break;
		} else if (cmp < 0) {
			lo = mid+1;
			fn = &fn_index[mid];
		} else {
			hi = mid;
		}
	}

	*fn_id = fn->idx;
	*offset = (i64)pc - (i64)fn->ptr;
}

void *migration_clock(void *_ign) {
	usleep(100);
	migration_flag = true;
	printf("[migration thread woke up]\n");
	while(1)
		usleep(100000);
	return NULL;
}

void m3_migration_init() {
	pthread_t tid;
	pthread_create(&tid, NULL, migration_clock, stdin);
}

M3Result m3_dump_state(d_m3OpSig) {
	IM3Runtime runtime = _mem->runtime;
	printf("[dumping state]\n");
	printf( "_pc:\t%p\n"
	        "_sp:\t%p\n"
	        "_mem:\t%p\n"
			"_r0:\t%ld\n"
			"_fp0:\t%lf\n"
			"_cs:\t%p\n"
			"stack:\t%p\n"
			"callstack:\t%p\n",
			d_m3OpAllArgs, runtime->stack, runtime->callStack);

	build_fn_index(runtime);	

	FILE *out = fopen("wasm3dump", "w");

	u32 fn_id, pc_offset, sp_offset;

	find_fn(_pc, &fn_id, &pc_offset);
	fwrite(&fn_id, sizeof(fn_id), 1, out);
	fwrite(&pc_offset, sizeof(pc_offset), 1, out);
	
	sp_offset = (i64)_sp - (i64)runtime->stack;
	fwrite(&sp_offset, sizeof(sp_offset), 1, out);
	// fwrite(&_mem, sizeof(_mem), 1, out);
	fwrite(&_r0, sizeof(_r0), 1, out);
	fwrite(&_fp0, sizeof(_fp0), 1, out);
	u32 cs_size = _cs - runtime->callStack;
	fwrite(&cs_size, sizeof(cs_size), 1, out);

	// value stack
	fwrite(&runtime->numStackSlots, sizeof(runtime->numStackSlots), 1, out);
	fwrite(runtime->stack, sizeof(m3reg_t), runtime->numStackSlots, out);

	// memory
	fwrite(&runtime->memory.numPages, sizeof(runtime->memory.numPages), 1, out);
	fwrite(runtime->memory.mallocated+1,
	       c_m3MemPageSize, runtime->memory.numPages, out);
	
	// call stack
	_cs--;
	while (_cs != runtime->callStack) {
		u32 cs_fn_id, offset;
		find_fn(_cs->pc, &cs_fn_id, &offset);
		_cs->fn_id = cs_fn_id;
		_cs->pc_offset = offset;
		_cs->sp_offset = (i64)_cs->sp - (i64)runtime->stack;
		_cs--;
	}
	fwrite(runtime->callStack, sizeof(cs_frame), cs_size, out);

	fclose(out);
	printf(
		"pc offset: %d\n"
		"sp offset: %d\n"
		"cs offset: %d\n",
		pc_offset,
		sp_offset,
		cs_size
	);
	exit(0);
}

// TODO: worry about allocations when loading memory and stack 
M3Result m3_resume_state(IM3Runtime runtime) {
	M3Result result = c_m3Err_none;

	pc_t _pc;
	u64 *_sp;
	M3MemoryHeader *_mem = runtime->memory.mallocated;
	m3reg_t _r0;
	f64 _fp0;
	cs_frame * _cs;

	u32 fn_id, pc_offset, sp_offset, cs_size;

	FILE *f = fopen("wasm3dump", "r");

	// load basic registers
	fread(&fn_id, sizeof(fn_id), 1, f);
	fread(&pc_offset, sizeof(pc_offset), 1, f);
	fread(&sp_offset, sizeof(sp_offset), 1, f);
	_sp = (char *)runtime->stack + sp_offset;
	// fread(&ignore, sizeof(ignore), 1, f);
	fread(&_r0, sizeof(_r0), 1, f);
	fread(&_fp0, sizeof(_fp0), 1, f);
	fread(&cs_size, sizeof(cs_size), 1, f);

	// load stack
	u32 stack_size;
	fread(&stack_size, sizeof(stack_size), 1, f);
	assert(stack_size <= runtime->numStackSlots);
	fread(runtime->stack, sizeof(m3reg_t), stack_size, f);

	// load memory
	u32 mem_pages;
	fread(&mem_pages, sizeof(mem_pages), 1, f);
	assert(mem_pages <= runtime->memory.numPages);
	fread(runtime->memory.mallocated+1,
	      mem_pages * c_m3MemPageSize, 1, f);

	// load cs & call stack
	assert(cs_size <= runtime->callStackSize);
	fread(runtime->callStack, sizeof(cs_frame), cs_size, f);
	_cs = runtime->callStack + cs_size;

	// Compile functions on the call stack
	IM3Function functions = runtime->modules[0].functions;
	for (cs_frame * cf = runtime->callStack + 1; cf != _cs; cf++) {
		if (not functions[cf->fn_id].compiled)
        	result = Compile_Function(functions+cf->fn_id);
		cf->pc = (u64)functions[cf->fn_id].compiled+cf->pc_offset;
		cf->sp = (char *)runtime->stack + cf->sp_offset;
	}
	_pc = (u64)functions[fn_id].compiled + pc_offset;

	printf("[resumed state]\n");
	printf( "_pc: %p\n"
	        "_sp: %p\n"
	        "_mem: %p\n"
			"_r0: %ld\n"
			"_fp0: %lf\n"
			"_cs: %p\n"
			"stack: %p\n",
			d_m3OpAllArgs, runtime->stack);
	
	jmp_start(d_m3OpAllArgs);
	u64 * stack = runtime->stack;
	printf("[%ld, %ld, ...]\n", stack[0], stack[1]);
	exit(0);
	return NULL;
}
