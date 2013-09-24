
include(CheckFunctionExists)
check_function_exists(getopt_long HAVE_GETOPT_LONG)

if (TOOLS)
    if (NOT HAVE_GETOPT_LONG)
        message(SEND_ERROR "getopt_long required when compiling tools")
    endif (NOT HAVE_GETOPT_LONG)

    find_library(ZLIB_LIBRARIES
        NAMES z
        PATHS ${ZLI_LIB})
    if (NOT ZLIB_LIBRARIES)
        message(SEND_ERROR "Zlib library not found")
    endif (NOT ZLIB_LIBRARIES)
endif (TOOLS)

if (BPKFS)
    include(FindPkgConfig)
    pkg_check_modules(FUSE REQUIRED fuse)
    string(REPLACE ";" " " FUSE_CFLAGS "${FUSE_CFLAGS}")
endif (BPKFS)

if (TESTS)
    enable_language(CXX)
    include(FindPkgConfig)
    pkg_check_modules(CPPUNIT cppunit)
    if (NOT CPPUNIT_FOUND)
        find_library(CPPUNIT_LIBRARIES
             NAMES cppunit
             PATHS ${CPPUNIT_LIB})
        set(CPPUNIT_INCLUDE_DIRS ${CPPUNIT_INCLUDE_DIRS})
        if (NOT CPPUNIT_LIBRARIES)
            message(SEND_ERROR "CppUnit library not found")
        endif (NOT CPPUNIT_LIBRARIES)
    endif(NOT CPPUNIT_FOUND)
    enable_testing()
endif (TESTS)

include(CheckIncludeFile)
CHECK_INCLUDE_FILE(sys/queue.h HAVE_SYS_QUEUE)

include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(htobe32 endian.h HAVE_ENDIAN_FUNCS)

