#include <stdio.h>
#include <time.h>

#define FATAL(msg, ...) { printf("Fatal: " msg "\n", ##__VA_ARGS__); return; }

#include "m3/m3.h"
#include "m3/m3_env.h"

#include "m3/extra/fib32.wasm.h"

void run_wasm()
{
    M3Result result = c_m3Err_none;

    uint8_t* wasm = (uint8_t*)fib32_wasm;
    size_t fsize = fib32_wasm_len-1;

    printf("Loading WebAssembly...\n");

    IM3Environment env = m3_NewEnvironment ();
    if (!env) FATAL("m3_NewEnvironment failed");

    IM3Runtime runtime = m3_NewRuntime (env, 1024, NULL);
    if (!runtime) FATAL("m3_NewRuntime failed");

    IM3Module module;
    result = m3_ParseModule (env, &module, wasm, fsize);
    if (result) FATAL("m3_ParseModule: %s", result);

    result = m3_LoadModule (runtime, module);
    if (result) FATAL("m3_LoadModule: %s", result);

    IM3Function f;
    result = m3_FindFunction (&f, runtime, "fib");
    if (result) FATAL("m3_FindFunction: %s", result);

    printf("Running...\n");

    const char* i_argv[2] = { "24", NULL };
    result = m3_CallWithArgs (f, 1, i_argv);

    if (result) FATAL("m3_CallWithArgs: %s", result);

    long value = *(uint64_t*)(runtime->stack);
    printf("Result: %ld\n", value);
}

int main() {
    printf("\nwasm3 on HiFive1, build " __DATE__ " " __TIME__ "\n");
    // TODO: fix clock (shows wrong time)
    clock_t start = clock();
    run_wasm();
    clock_t end = clock();

    printf("Elapsed: %d ms\n", (end - start)*1000 / CLOCKS_PER_SEC);
}
