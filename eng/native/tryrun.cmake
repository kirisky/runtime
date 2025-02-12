set(CROSS_ROOTFS $ENV{ROOTFS_DIR})
set(TARGET_ARCH_NAME $ENV{TARGET_BUILD_ARCH})

# Also allow building as Android without specifying `-cross`.
if(NOT DEFINED TARGET_ARCH_NAME AND DEFINED ANDROID_BUILD)
  if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(TARGET_ARCH_NAME "arm64")
  elseif(ANDROID_ABI STREQUAL "x86_64")
    set(TARGET_ARCH_NAME "x64")
  elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(TARGET_ARCH_NAME "arm")
  elseif(ANDROID_ABI STREQUAL "x86")
    set(TARGET_ARCH_NAME "x86")
  else()
    message(FATAL_ERROR "ANDROID_ABI ${ANDROID_ABI} not recognized!")
  endif()
endif()

macro(set_cache_value)
  set(${ARGV0} ${ARGV1} CACHE STRING "Result from TRY_RUN" FORCE)
  set(${ARGV0}__TRYRUN_OUTPUT "dummy output" CACHE STRING "Output from TRY_RUN" FORCE)
endmacro()

if(NOT DEFINED ANDROID_BUILD)
  if(EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/armv7-alpine-linux-musleabihf OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/armv6-alpine-linux-musleabihf OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/aarch64-alpine-linux-musl OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/s390x-alpine-linux-musl OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/ppc64le-alpine-linux-musl OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/i586-alpine-linux-musl OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/x86_64-alpine-linux-musl OR
    EXISTS ${CROSS_ROOTFS}/usr/lib/gcc/riscv64-alpine-linux-musl)

    set(ALPINE_LINUX 1)
  elseif(EXISTS ${CROSS_ROOTFS}/bin/freebsd-version)
    set(FREEBSD 1)
    set(CMAKE_SYSTEM_NAME FreeBSD)
    set(CLR_CMAKE_TARGET_OS freebsd)
  elseif(EXISTS ${CROSS_ROOTFS}/usr/platform/i86pc)
    set(ILLUMOS 1)
    set(CLR_CMAKE_TARGET_OS sunos)
  elseif(EXISTS /System/Library/CoreServices)
    set(DARWIN 1)
  elseif(EXISTS ${CROSS_ROOTFS}/etc/tizen-release)
    set(TIZEN 1)
  elseif(EXISTS ${CROSS_ROOTFS}/boot/system/develop/headers/config/HaikuConfig.h)
    set(HAIKU 1)
    set(CLR_CMAKE_TARGET_OS haiku)
  endif()
endif()

if(DARWIN AND NOT DEFINED ANDROID_BUILD)
  if(TARGET_ARCH_NAME MATCHES "^(arm64|x64)$")
    set_cache_value(HAS_POSIX_SEMAPHORES_EXITCODE 1)
    set_cache_value(HAVE_BROKEN_FIFO_KEVENT_EXITCODE 1)
    set_cache_value(HAVE_BROKEN_FIFO_SELECT_EXITCODE 1)
    set_cache_value(HAVE_CLOCK_MONOTONIC_COARSE_EXITCODE 1)
    set_cache_value(HAVE_CLOCK_MONOTONIC_EXITCODE 0)
    set_cache_value(HAVE_CLOCK_REALTIME_EXITCODE 0)
    set_cache_value(HAVE_CLOCK_THREAD_CPUTIME_EXITCODE 0)
    set_cache_value(HAVE_CLOCK_GETTIME_NSEC_NP_EXITCODE 0)
    set_cache_value(HAVE_FUNCTIONAL_PTHREAD_ROBUST_MUTEXES_EXITCODE 1)
    set_cache_value(HAVE_MMAP_DEV_ZERO_EXITCODE 1)
    set_cache_value(HAVE_PROCFS_CTL_EXITCODE 1)
    set_cache_value(HAVE_PROCFS_STAT_EXITCODE 1)
    set_cache_value(HAVE_PROCFS_STATM_EXITCODE 1)
    set_cache_value(HAVE_SCHED_GETCPU_EXITCODE 1)
    set_cache_value(HAVE_SCHED_GET_PRIORITY_EXITCODE 0)
    set_cache_value(HAVE_WORKING_CLOCK_GETTIME_EXITCODE 0)
    set_cache_value(HAVE_WORKING_GETTIMEOFDAY_EXITCODE 0)
    set_cache_value(MMAP_ANON_IGNORES_PROTECTION_EXITCODE 1)
    set_cache_value(ONE_SHARED_MAPPING_PER_FILEREGION_PER_PROCESS_EXITCODE 1)
    set_cache_value(PTHREAD_CREATE_MODIFIES_ERRNO_EXITCODE 1)
    set_cache_value(REALPATH_SUPPORTS_NONEXISTENT_FILES_EXITCODE 1)
    set_cache_value(SEM_INIT_MODIFIES_ERRNO_EXITCODE 1)
    set_cache_value(HAVE_SHM_OPEN_THAT_WORKS_WELL_ENOUGH_WITH_MMAP_EXITCODE 1)
  else()
    message(FATAL_ERROR "Arch is ${TARGET_ARCH_NAME}. Only arm64 or x64 is supported for OSX cross build!")
  endif()
elseif(DEFINED ANDROID_BUILD OR TARGET_ARCH_NAME MATCHES "^(armel|arm|armv6|arm64|loongarch64|riscv64|s390x|ppc64le|x86|x64)$" OR FREEBSD OR ILLUMOS OR TIZEN OR HAIKU)
  set_cache_value(HAS_POSIX_SEMAPHORES_EXITCODE 0)
  set_cache_value(HAVE_CLOCK_MONOTONIC_COARSE_EXITCODE 0)
  set_cache_value(HAVE_CLOCK_MONOTONIC_EXITCODE 0)
  set_cache_value(HAVE_CLOCK_REALTIME_EXITCODE 0)
  set_cache_value(HAVE_CLOCK_THREAD_CPUTIME_EXITCODE 0)
  set_cache_value(HAVE_MMAP_DEV_ZERO_EXITCODE 0)
  set_cache_value(HAVE_PROCFS_CTL_EXITCODE 1)
  set_cache_value(HAVE_PROCFS_STAT_EXITCODE 0)
  set_cache_value(HAVE_PROCFS_STATM_EXITCODE 0)
  set_cache_value(HAVE_SCHED_GETCPU_EXITCODE 0)
  set_cache_value(HAVE_SCHED_GET_PRIORITY_EXITCODE 0)
  set_cache_value(HAVE_WORKING_CLOCK_GETTIME_EXITCODE 0)
  set_cache_value(HAVE_WORKING_GETTIMEOFDAY_EXITCODE 0)
  set_cache_value(ONE_SHARED_MAPPING_PER_FILEREGION_PER_PROCESS_EXITCODE 1)
  set_cache_value(PTHREAD_CREATE_MODIFIES_ERRNO_EXITCODE 1)
  set_cache_value(REALPATH_SUPPORTS_NONEXISTENT_FILES_EXITCODE 1)
  set_cache_value(SEM_INIT_MODIFIES_ERRNO_EXITCODE 1)

  if(ALPINE_LINUX)
    set_cache_value(HAVE_SHM_OPEN_THAT_WORKS_WELL_ENOUGH_WITH_MMAP_EXITCODE 1)
  else()
    set_cache_value(HAVE_SHM_OPEN_THAT_WORKS_WELL_ENOUGH_WITH_MMAP_EXITCODE 0)
  endif()

  if (FREEBSD)
    set_cache_value(HAVE_SHM_OPEN_THAT_WORKS_WELL_ENOUGH_WITH_MMAP 1)
    set_cache_value(HAVE_CLOCK_MONOTONIC 1)
    set_cache_value(HAVE_CLOCK_REALTIME 1)
    set_cache_value(HAVE_BROKEN_FIFO_KEVENT_EXITCODE 1)
    set_cache_value(HAVE_PROCFS_STAT 0)
    set_cache_value(HAVE_PROCFS_STATM 0)
    set_cache_value(HAVE_FUNCTIONAL_PTHREAD_ROBUST_MUTEXES_EXITCODE 0)
  elseif(ILLUMOS)
    set_cache_value(HAVE_PROCFS_CTL_EXITCODE 0)
    set_cache_value(COMPILER_SUPPORTS_W_CLASS_MEMACCESS 1)
    set_cache_value(HAVE_SET_MAX_VARIABLE 1)
    set_cache_value(HAVE_FULLY_FEATURED_PTHREAD_MUTEXES 1)
    set_cache_value(HAVE_FUNCTIONAL_PTHREAD_ROBUST_MUTEXES_EXITCODE 0)
  elseif (TIZEN)
    set_cache_value(HAVE_FUNCTIONAL_PTHREAD_ROBUST_MUTEXES_EXITCODE 0)
  elseif(HAIKU)
    set_cache_value(HAVE_CLOCK_MONOTONIC_COARSE_EXITCODE 1)
    set_cache_value(HAVE_PROCFS_STAT_EXITCODE 1)
    set_cache_value(HAVE_PROCFS_STATM_EXITCODE 1)
  elseif(ANDROID_BUILD)
    set_cache_value(ONE_SHARED_MAPPING_PER_FILEREGION_PER_PROCESS 0)
  endif()
elseif(NOT WIN32)
  message(FATAL_ERROR "Unsupported platform. OS: ${CMAKE_SYSTEM_NAME}, arch: ${TARGET_ARCH_NAME}")
endif()

if(TARGET_ARCH_NAME MATCHES "^(x86|x64|s390x|armv6|loongarch64|riscv64|ppc64le)$")
  set_cache_value(HAVE_FUNCTIONAL_PTHREAD_ROBUST_MUTEXES_EXITCODE 0)
endif()
