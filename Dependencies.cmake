
include(CheckFunctionExists)
check_function_exists(getopt_long HAVE_GETOPT_LONG)

if (TOOLS)
    if (NOT HAVE_GETOPT_LONG)
        message(SEND_ERROR "getopt_long required when compiling tools")
    endif (NOT HAVE_GETOPT_LONG)
endif (TOOLS)

if (BPKFS)
    include(FindPkgConfig)
    pkg_check_modules(FUSE REQUIRED fuse)
    string(REPLACE ";" " " FUSE_CFLAGS "${FUSE_CFLAGS}")
endif (BPKFS)

include(CheckIncludeFile)
CHECK_INCLUDE_FILE(sys/queue.h HAVE_SYS_QUEUE)

include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(htobe32 endian.h HAVE_ENDIAN_FUNCS)

