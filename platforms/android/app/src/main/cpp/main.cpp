#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "m3/m3.h"
#include "m3/m3_env.h"
#include "m3/m3_config.h"
#include "m3/m3_api_wasi.h"
#include "m3/m3_api_libc.h"

#include "m3/extra/fib32.wasm.h"

#include <jni.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include <thread>
#include <unistd.h>


#define FATAL(msg, ...) { __android_log_print(ANDROID_LOG_DEBUG, "Fatal", msg "\n", ##__VA_ARGS__); return; }

extern "C" {
void handle_cmd(struct android_app *pApp, int32_t cmd) {
}


void run_wasm(android_app *pApp) {
    int pfd[2];
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);
    close(pfd[1]);
    char res_buf[128];

    AAsset *asset = AAssetManager_open(
            pApp->activity->assetManager,
            "matmul2.wasm",
            AASSET_MODE_STREAMING);
    const uint8_t *wasm = (const uint8_t *)AAsset_getBuffer(asset);
    size_t fsize = AAsset_getLength(asset);

    M3Result result = c_m3Err_none;

//    uint8_t *wasm = (uint8_t *) fib32_wasm;
//    size_t fsize = fib32_wasm_len - 1;

    __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Loading WebAssembly...\n");

    IM3Environment env = m3_NewEnvironment();
    if (!env) FATAL("m3_NewEnvironment failed");

    IM3Runtime runtime = m3_NewRuntime(env, 8192, NULL);
    if (!runtime) FATAL("m3_NewRuntime failed");

    IM3Module module;
    result = m3_ParseModule(env, &module, wasm, fsize);
    if (result) FATAL("m3_ParseModule: %s", result);

    result = m3_LoadModule(runtime, module);
    if (result) FATAL("m3_LoadModule: %s", result);

    result = m3_LinkWASI (runtime->modules);
    if (result) FATAL("m3_LinkWASI: %s", result);

    result = m3_LinkLibC (runtime->modules);
    if (result) FATAL("m3_LinkLibC: %s", result);


    IM3Function f;
    result = m3_FindFunction(&f, runtime, "_start");
    if (result) FATAL("m3_FindFunction: %s", result);

    result = m3_Call(f);
    if (result) FATAL("m3_Call: %s", result);

    long value = *(uint64_t *) (runtime->stack);
    __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Result: %ld\n", value);

    close(1);
    close(2);
    __android_log_print(ANDROID_LOG_DEBUG, "LOG", "closed stdout/err\n");
    read(pfd[0], res_buf, 127);
    res_buf[127] = '\0';
    __android_log_print(ANDROID_LOG_DEBUG, "OUTPUT", "-start\n%s\nend-\n", res_buf);
}

void android_main(android_app *pApp) {

    std::thread(run_wasm, pApp).detach();
    pApp->onAppCmd = handle_cmd;
    int events;
    struct android_poll_source *pSource;
    do {
        if (ALooper_pollAll(0, NULL, &events, (void **) &pSource) >= 0) {
            if (pSource) {
                pSource->process(pApp, pSource);
            }
        }
    } while (!pApp->destroyRequested);
}
}
