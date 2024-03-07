set(OLOG_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

include_directories(${OLOG_SOURCE_DIR})

find_package(Catch2 3 REQUIRED)

set(TESTS_COMPILE_OPTIONS -std=c++17 -g -Wformat -Werror -fsanitize=address)
set(TESTS_LINK_LIBRARIES Catch2::Catch2WithMain asan)

add_compile_options(${TESTS_COMPILE_OPTIONS})

add_library(olog_buffers ${OLOG_SOURCE_DIR}/buffers.cc)
add_library(olog_utils ${OLOG_SOURCE_DIR}/utils.cc)
add_library(olog_log_info ${OLOG_SOURCE_DIR}/log_info.cc)
add_library(olog_logger ${OLOG_SOURCE_DIR}/logger.cc)


add_executable(buffers_test buffers_test.cc)
add_executable(utils_test utils_test.cc)
add_executable(log_info_test log_info_test.cc)
add_executable(olog_test olog_test.cc)

# 启用调试输出信息。
target_compile_definitions(olog_log_info PUBLIC OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING)
target_compile_definitions(olog_logger PUBLIC OLOG_ENABLE_LOGGER_DEBUG_PRINTTING)

target_link_libraries(olog_logger olog_buffers olog_log_info uring)
target_link_libraries(buffers_test olog_buffers ${TESTS_LINK_LIBRARIES})
target_link_libraries(utils_test olog_utils ${TESTS_LINK_LIBRARIES})
target_link_libraries(log_info_test olog_log_info ${TESTS_LINK_LIBRARIES})
target_link_libraries(olog_test olog_buffers olog_utils olog_log_info olog_logger ${TESTS_LINK_LIBRARIES})

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