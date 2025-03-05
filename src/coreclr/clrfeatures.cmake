if(CLR_CMAKE_TARGET_TIZEN_LINUX)
  set(FEATURE_GDBJIT_LANGID_CS 1)
endif()

if(NOT DEFINED FEATURE_EVENT_TRACE)
  if (NOT CLR_CMAKE_TARGET_BROWSER)
    # To actually disable FEATURE_EVENT_TRACE, also change clr.featuredefines.props
    set(FEATURE_EVENT_TRACE 1)
  endif()
endif(NOT DEFINED FEATURE_EVENT_TRACE)

if(NOT DEFINED FEATURE_PERFTRACING AND FEATURE_EVENT_TRACE)
  set(FEATURE_PERFTRACING 1)
endif(NOT DEFINED FEATURE_PERFTRACING AND FEATURE_EVENT_TRACE)

if(NOT DEFINED FEATURE_DBGIPC)
  if(CLR_CMAKE_TARGET_UNIX)
    set(FEATURE_DBGIPC 1)
  endif()
endif(NOT DEFINED FEATURE_DBGIPC)

if(NOT DEFINED FEATURE_INTERPRETER)
  if(CLR_CMAKE_TARGET_ARCH_AMD64 OR CLR_CMAKE_TARGET_ARCH_ARM64)
    set(FEATURE_INTERPRETER $<IF:$<CONFIG:Debug,Checked>,1,0>)
  else(CLR_CMAKE_TARGET_ARCH_AMD64 OR CLR_CMAKE_TARGET_ARCH_ARM64)
    set(FEATURE_INTERPRETER 0)
  endif(CLR_CMAKE_TARGET_ARCH_AMD64 OR CLR_CMAKE_TARGET_ARCH_ARM64)
endif(NOT DEFINED FEATURE_INTERPRETER)

if(NOT DEFINED FEATURE_STANDALONE_GC)
  set(FEATURE_STANDALONE_GC 1)
endif(NOT DEFINED FEATURE_STANDALONE_GC)

if(NOT DEFINED FEATURE_AUTO_TRACE)
  set(FEATURE_AUTO_TRACE 0)
endif(NOT DEFINED FEATURE_AUTO_TRACE)

if(NOT DEFINED FEATURE_SINGLE_FILE_DIAGNOSTICS)
  set(FEATURE_SINGLE_FILE_DIAGNOSTICS 1)
endif(NOT DEFINED FEATURE_SINGLE_FILE_DIAGNOSTICS)

if (CLR_CMAKE_TARGET_WIN32 OR CLR_CMAKE_TARGET_UNIX)
  set(FEATURE_COMWRAPPERS 1)
endif()

if (CLR_CMAKE_TARGET_APPLE)
  set(FEATURE_OBJCMARSHAL 1)
endif()

if (CLR_CMAKE_TARGET_WIN32)
  set(FEATURE_TYPEEQUIVALENCE 1)
endif(CLR_CMAKE_TARGET_WIN32)


if (CLR_CMAKE_TARGET_MACCATALYST OR CLR_CMAKE_TARGET_IOS OR CLR_CMAKE_TARGET_TVOS)
  set(FEATURE_CORECLR_CACHED_INTERFACE_DISPATCH 1)
  set(FEATURE_CORECLR_VIRTUAL_STUB_DISPATCH 0)
else()
  # Enable cached interface dispatch so that we can test/debug it more easily on non-embedded scenarios (set DOTNET_UseCachedInterfaceDispatch=1)
  # Only enable in chk/debug builds as this support isn't intended for retail use elsewhere
  if (CLR_CMAKE_TARGET_ARCH_AMD64 OR CLR_CMAKE_TARGET_ARCH_ARM64)
    set(FEATURE_CORECLR_CACHED_INTERFACE_DISPATCH $<IF:$<CONFIG:Debug,Checked>,1,0>)
  else()
    set(FEATURE_CORECLR_CACHED_INTERFACE_DISPATCH 0)
  endif()
  set(FEATURE_CORECLR_VIRTUAL_STUB_DISPATCH 1)
endif()

if (CLR_CMAKE_HOST_UNIX AND CLR_CMAKE_HOST_ARCH_AMD64)
  # Allow 16 byte compare-exchange (cmpxchg16b)
  add_compile_options($<${FEATURE_CORECLR_CACHED_INTERFACE_DISPATCH}:-mcx16>)
endif()
