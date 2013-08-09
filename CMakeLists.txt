cmake_minimum_required(VERSION 2.8)

project(libbpk C CXX)

include(GitVersion.cmake)

set(libbpk_VER_MAJOR 1)
set(libbpk_VER_MINOR 0)
set(libbpk_VER_PATCH "${GIT_COUNT}")
set(libbpk_VERSION "${libbpk_VER_MAJOR}.${libbpk_VER_MINOR}.${libbpk_VER_PATCH}")

option(TOOLS "Compile command line tools" ON)
option(TESTS "Compile and run unit tests" OFF)

if (TESTS)
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

include(Dependencies.cmake)

configure_file("config.h.in" "config.h")
configure_file("version.h.in" "version.h")

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools)
add_subdirectory(doc)

