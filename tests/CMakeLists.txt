include_directories(${OLOG_SOURCE_DIR})

find_package(Catch2 3 REQUIRED)

set(TESTS_LINK_LIBRARIES Catch2::Catch2WithMain)

add_executable(buffers_test buffers_test.cc)
add_executable(utils_test utils_test.cc)
add_executable(log_info_test log_info_test.cc)
add_executable(olog_test olog_test.cc)

target_link_libraries(buffers_test olog_debug ${TESTS_LINK_LIBRARIES})
target_link_libraries(utils_test olog_debug ${TESTS_LINK_LIBRARIES})
target_link_libraries(log_info_test olog_debug ${TESTS_LINK_LIBRARIES})
target_link_libraries(olog_test olog_debug ${TESTS_LINK_LIBRARIES})

add_test(
    NAME buffers_test
    COMMAND buffers_test
)

add_test(
    NAME utils_test
    COMMAND utils_test
)

add_test(
    NAME log_info_test
    COMMAND log_info_test
)

add_test(
    NAME olog_test
    COMMAND olog_test
)