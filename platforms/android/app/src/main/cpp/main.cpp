#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <thread>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

#include <jni.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "m3/m3.h"
#include "m3/m3_env.h"
#include "m3/m3_config.h"
#include "m3/m3_api_wasi.h"
#include "m3/m3_api_libc.h"

#define FATAL(msg, ...) { __android_log_print(ANDROID_LOG_DEBUG, "Fatal", msg "\n", ##__VA_ARGS__); return; }

extern "C" {
void handle_cmd(struct android_app *pApp, int32_t cmd) {
}

void run_function(android_app *pApp, char *filename, char *funcname, int argc, char *argv[]) {
    int pfd[2];
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);
    close(pfd[1]);
    char res_buf[256];

    AAsset *asset = AAssetManager_open(
            pApp->activity->assetManager,
            filename,
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
    result = m3_FindFunction(&f, runtime, funcname);
    if (result) FATAL("m3_FindFunction: %s", result);

    result = m3_CallWithArgs(f, argc, argv);
    if (result) FATAL("m3_Call: %s", result);

    long value = *(uint64_t *) (runtime->stack);
    __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Result: %ld\n", value);

    close(1);
    close(2);
    __android_log_print(ANDROID_LOG_DEBUG, "LOG", "closed stdout/err\n");
    size_t bytes = read(pfd[0], res_buf, 255);
    res_buf[bytes] = '\0';
    __android_log_print(ANDROID_LOG_DEBUG, "OUTPUT", "-start\n%s\nend-\n", res_buf);
}

void run_wasm(android_app *pApp) {
    char *argv = "coremark-wasi.wasm";
    run_function(pApp, "coremark-wasi.wasm", "_start", 1, &argv);
//
//    argv = "40";
//    run_function(pApp, "fib.wasm", "fib", 1, &argv);
//
//    argv = "20000";
//    run_function(pApp, "isort.wasm", "mymain", 1, &argv);
//
//    argv = "600";
//    run_function(pApp, "matmul.wasm", "mymain", 1, &argv);
}

void rcv_code() {
    int sockfd, connfd, len;
    sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "socket creation failed...\n");
        return;
    }
    else
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8000);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "socket bind failed...\n");
        return;
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Socket successfully binded..\n");
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Listen failed...\n");
        return;
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "Server listening..\n");
    }

    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (sockaddr*)&cli, (socklen_t *)&len);
    if (connfd < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "server acccept failed...\n");
        return;
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "LOG", "server acccept the client...\n");
    }

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
