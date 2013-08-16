
link_directories(
    ${CMAKE_BINARY_DIR}/src
    )
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    )

if (TOOLS)
    set(mkbpk_SRCS
        mkbpk.c)

    add_executable(mkbpk
        ${mkbpk_SRCS})
    target_link_libraries(mkbpk
        bpk)
    install(TARGETS mkbpk
        RUNTIME DESTINATION bin COMPONENT Runtime)
endif (TOOLS)

if (BPKFS)
    set(bpkfs_SRCS
        bpkfs.c bpkfs.h)

    add_executable(bpkfs
        ${bpkfs_SRCS})
    target_link_libraries(bpkfs
        bpk ${FUSE_LIBRARIES} pthread)
    set_target_properties(bpkfs PROPERTIES
        COMPILE_FLAGS "${FUSE_CFLAGS_OTHER}"
        PUBLIC_HEADER "bpkfs.h")
    install(TARGETS bpkfs
        RUNTIME DESTINATION bin COMPONENT Runtime
        PUBLIC_HEADER DESTINATION include/${PROJECT_NAME} COMPONENT Devel)
endif (BPKFS)

