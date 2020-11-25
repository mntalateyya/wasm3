#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#endif

#include "m3.h"
#include "m3_env.h"
#include "fn_index.h"

#ifdef WIN32
//https://stackoverflow.com/questions/5801813/c-usleep-is-obsolete-workarounds-for-windows-mingw
void usleep(__int64 usec) 
{ 
    HANDLE timer; 
    LARGE_INTEGER ft; 

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
}
#endif

static const char *StateFile = NULL;
static volatile bool migration_flag = false;
static jmp_buf jmp_env;

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

void *migration_clock(void *_ign) {
	char *tosleep = getenv("SLEEP");
	int i_tosleep = atoi(tosleep);
	usleep(i_tosleep);
	printf("[migration thread woke up]\n");
	migration_flag = true;
	return NULL;
}

void m3_migration_init(const char *stateFile) {
	StateFile = stateFile;
#ifdef WIN32
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)migration_clock, NULL, 0, NULL);
#else
	pthread_t tid;
	pthread_create(&tid, NULL, migration_clock, NULL);
#endif
}

// https://stackoverflow.com/a/44896326/7119758
long long millitime(void) {
#ifdef WIN32
	return GetTickCount();
#else
	struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000000) + tv.tv_usec;
#endif
}

M3Result m3_dump_state(d_m3OpSig) {

	long long start = millitime();

	IM3Runtime runtime = _mem->runtime;

	build_fn_index(runtime);	

	FILE *out = fopen(StateFile, "wb");

	u64 fn_id, pc_offset, sp_offset;
	find_fn(_pc, &fn_id, &pc_offset);
	fwrite(&fn_id, sizeof(fn_id), 1, out);
	fwrite(&pc_offset, sizeof(pc_offset), 1, out);
	
	sp_offset = (u64)_sp - (u64)runtime->stack;
	fwrite(&sp_offset, sizeof(sp_offset), 1, out);
	// fwrite(&_mem, sizeof(_mem), 1, out);
	fwrite(&_r0, sizeof(_r0), 1, out);
	fwrite(&_fp0, sizeof(_fp0), 1, out);
	u64 cs_size = _cs - runtime->callStack;
	fwrite(&cs_size, sizeof(cs_size), 1, out);

	assert(sizeof(_r0) == 8 && sizeof(_fp0) == 8);

	// value stack
	m3reg_t *sp_end = (m3reg_t *)runtime->stack + runtime->numStackSlots;
	while (sp_end > (m3reg_t*)runtime->stack && !sp_end[-1])
		sp_end--;
	u64 stack_size = sp_end - (m3reg_t *)runtime->stack;
	fwrite(&stack_size, sizeof(stack_size), 1, out);
	fwrite(runtime->stack, sizeof(m3reg_t), stack_size, out);
	
	// memory
	u64 numpages = runtime->memory.numPages;
	fwrite(&numpages, sizeof(numpages), 1, out);
	fwrite(runtime->memory.mallocated+1,
	       c_m3MemPageSize, runtime->memory.numPages, out);
	
	// call stack
	_cs--;
	while (_cs != runtime->callStack) {
		u64 cs_fn_id, offset;
		find_fn(_cs->pc, &cs_fn_id, &offset);
		_cs->fn_id = cs_fn_id;
		_cs->pc_offset = offset;
		_cs->sp_offset = (i64)_cs->sp - (i64)runtime->stack;
		_cs--;
	}
	fwrite(runtime->callStack, sizeof(cs_frame), cs_size, out);

	printf("serialize-time: %lld\ntotal-size: %ld\nvcs-size: %lld\n",
		millitime()-start, ftell(out), cs_size);
	fclose(out);
	exit(0);
}

// TODO: worry about allocations when loading memory and stack 
M3Result m3_resume_state(IM3Runtime runtime) {
	long long start = millitime();

	M3Result result = c_m3Err_none;

	pc_t _pc;
	u64 *_sp;
	M3MemoryHeader *_mem;
	m3reg_t _r0;
	f64 _fp0;
	cs_frame * _cs;

	u64 fn_id, pc_offset, sp_offset, cs_size;

	FILE *f = fopen(StateFile, "rb");

	// load basic registers
	fread(&fn_id, sizeof(fn_id), 1, f);
	fread(&pc_offset, sizeof(pc_offset), 1, f);
	fread(&sp_offset, sizeof(sp_offset), 1, f);
	_sp = (u64*)((u64)runtime->stack + sp_offset);
	fread(&_r0, sizeof(_r0), 1, f);
	fread(&_fp0, sizeof(_fp0), 1, f);
	fread(&cs_size, sizeof(cs_size), 1, f);

	// load stack
	u64 stack_size;
	fread(&stack_size, sizeof(stack_size), 1, f);
	assert(stack_size <= runtime->numStackSlots);
	fread(runtime->stack, sizeof(m3reg_t), stack_size, f);

	// load memory
	u64 mem_pages;
	fread(&mem_pages, sizeof(mem_pages), 1, f);
	if (mem_pages > runtime->memory.numPages) {
		ResizeMemory(runtime, mem_pages);
	}
	fread(runtime->memory.mallocated+1,
	      mem_pages * c_m3MemPageSize, 1, f);
	_mem = runtime->memory.mallocated;

	// load cs & call stack
	assert(cs_size <= runtime->callStackSize);
	fread(runtime->callStack, sizeof(cs_frame), cs_size, f);
	_cs = runtime->callStack + cs_size;

	// Compile functions on the call stack
	IM3Function functions = runtime->modules[0].functions;
	for (cs_frame * cf = runtime->callStack + 1; cf != _cs; cf++) {
		if (not functions[cf->fn_id].compiled)
        	result = Compile_Function(functions+cf->fn_id);
		cf->pc = (pc_t)((u64)functions[cf->fn_id].compiled+cf->pc_offset);
		cf->sp = (u64*)((u64)runtime->stack + cf->sp_offset);
	}
	if (not functions[fn_id].compiled)
		result = Compile_Function(functions+fn_id);
	_pc = &functions[fn_id].compiled[pc_offset/sizeof(pc_t)];
	
	printf("deserial-time: %lld\n", millitime()-start);
	jmp_start(d_m3OpAllArgs);
	u64 * stack = runtime->stack;
	printf("Result: [%lld]\n", stack[0]);
	return NULL;
}
