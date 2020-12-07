cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DANDROID -Wno-format-security -O3
                -Dd_m3AllocateLinearMemory=1 -Dd_m3HasWASI
                #-fno-optimize-sibling-calls
                -fstrict-aliasing -flto -fno-fat-lto-objects)

file(GLOB M3_SRC FOLLOW_SYMLINKS "m3/*.c" "*.c" "*.cpp")

add_library(wasm3-jni SHARED ${M3_SRC})

set_target_properties(wasm3-jni PROPERTIES LINK_FLAGS "-flto -O3")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(native_app_glue STATIC
        ~/Android/Sdk/ndk/20.1.5948944/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
        ~/Android/Sdk/ndk/20.1.5948944/sources/android/native_app_glue)

target_link_libraries(
        wasm3-jni
        native_app_glue
        android
        log
)

#file(GLOB COREMARK FOLLOW_SYMLINKS "native/*.c" "native/linux64/*.c")
#add_executable(coremark ${COREMARK})
#target_include_directories(coremark PUBLIC native/linux64)
#target_include_directories(coremark PUBLIC native)
#target_compile_definitions(co  remark PUBLIC FLAGS_STR="-O3 -flto")