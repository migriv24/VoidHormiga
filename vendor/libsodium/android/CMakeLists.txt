# libsodium 1.0.20-stable for Android, without autotools: the source files and
# the defines libsodium's own build.zig uses for Linux, minus what Android's
# API 26 lacks (pthread_mutexattr_setprotocol, so no HAVE_PTHREAD_PRIO_INHERIT).
cmake_minimum_required(VERSION 3.20)
project(sodium_android C ASM)
set(S ${CMAKE_CURRENT_SOURCE_DIR}/libsodium-stable/src/libsodium)
file(GLOB_RECURSE SODIUM_SOURCES ${S}/*.c ${S}/*.S)
add_library(sodium STATIC ${SODIUM_SOURCES})
target_include_directories(sodium PRIVATE ${S}/include/sodium
                                  PUBLIC ${S}/include)
target_compile_definitions(sodium PRIVATE
  _GNU_SOURCE=1 CONFIGURED=1 DEV_MODE=1 HAVE_ATOMIC_OPS=1 HAVE_C11_MEMORY_FENCES=1
  HAVE_GCC_MEMORY_FENCES=1 HAVE_INLINE_ASM=1 HAVE_INTTYPES_H=1 HAVE_STDINT_H=1
  HAVE_TI_MODE=1 NATIVE_LITTLE_ENDIAN=1
  ASM_HIDE_SYMBOL=.hidden TLS=_Thread_local
  HAVE_CATCHABLE_ABRT=1 HAVE_CATCHABLE_SEGV=1 HAVE_CLOCK_GETTIME=1 HAVE_GETPID=1
  HAVE_MADVISE=1 HAVE_MLOCK=1 HAVE_MMAP=1 HAVE_MPROTECT=1 HAVE_NANOSLEEP=1
  HAVE_POSIX_MEMALIGN=1 HAVE_PTHREAD=1 HAVE_RAISE=1 HAVE_SYSCONF=1
  HAVE_SYS_AUXV_H=1 HAVE_SYS_MMAN_H=1 HAVE_SYS_PARAM_H=1 HAVE_SYS_RANDOM_H=1
  HAVE_WEAK_SYMBOLS=1 SODIUM_STATIC=1)
target_compile_options(sodium PRIVATE -O2 -fPIC -fvisibility=hidden -Wno-unknown-pragmas)

# a smoke test for the device: the two primitives Hormiga's vault uses
add_executable(sodium_smoke smoke.c)
target_link_libraries(sodium_smoke PRIVATE sodium)
target_compile_definitions(sodium_smoke PRIVATE SODIUM_STATIC=1)
