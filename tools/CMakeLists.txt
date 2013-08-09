
link_directories(
    ${CMAKE_BINARY_DIR}/src)
include_directories(
    ${CMAKE_SOURCE_DIR}/src)

set(mkbpk_SRCS
    mkbpk.c)

add_executable(mkbpk
    ${mkbpk_SRCS})
target_link_libraries(mkbpk
    bpk)
install(TARGETS mkbpk
    RUNTIME DESTINATION bin COMPONENT Runtime)

