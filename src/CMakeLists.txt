
set(libbpk_SRCS
    bpk_priv.h
    bpk.c
    crc32.c crc32.h)

set(libbpk_PUBHDRS
    bpk.h)

add_library(bpk SHARED
    ${libbpk_SRCS} ${libbpk_PUBHDRS})
set_target_properties(bpk PROPERTIES
    PUBLIC_HEADER ${libbpk_PUBHDRS})
install(TARGETS bpk
    ARCHIVE DESTINATION lib COMPONENT Devel
    LIBRARY DESTINATION lib COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME} COMPONENT Devel)

