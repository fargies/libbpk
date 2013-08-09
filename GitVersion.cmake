
find_program(GIT_EXE NAMES git git.cmd PATHS
    ${GIT_DIR}
    ENV PATHS
    $ENV{GIT_DIR})

execute_process(
    COMMAND ${GIT_EXE} "log" "--pretty=format:"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_FILE ${CMAKE_BINARY_DIR}/.version
    )
add_custom_target(git_check ALL DEPENDS ${CMAKE_BINARY_DIR}/.version)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/.version
    COMMAND ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}
    DEPENDS ${CMAKE_SOURCE_DIR}/.git
    )

file(READ ${CMAKE_BINARY_DIR}/.version GIT_COUNT)
string(LENGTH "${GIT_COUNT}" GIT_COUNT)
file(WRITE ${CMAKE_BINARY_DIR}/.version ${GIT_COUNT})

message(STATUS "Count commits: ${GIT_COUNT}")

