
if (TESTS)

link_directories(
    ${CMAKE_BINARY_DIR}/src
    )
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    )

add_library(testHelper STATIC
    test_main.cpp
    )
target_link_libraries(testHelper ${CPPUNIT_LIBRARIES})

set(test_SRCS
    test_ops.cpp
    )

add_executable(test_all ${test_SRCS})
target_link_libraries(test_all testHelper bpk)
install(TARGETS test_all
    RUNTIME DESTINATION tests/${PROJECT_NAME} COMPONENT tests)

add_test(test_all ${CMAKE_CURRENT_BINARY_DIR}/test_all)

endif (TESTS)

