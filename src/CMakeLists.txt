set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OLOG_RELEASE_COMPILE_FLAGS -O2 -DNDEBUG)
set(OLOG_DEBUG_COMPILE_FLAGS -g -fsanitize=address -fsanitize=leak -fsanitize=undefined)
set(OLOG_MT_DEBUG_COMPILE_FLAGS -g  -fsanitize=thread -fsanitize=undefined)

set(OLOG_LINK_LIBRARIES uring)
set(OLOG_DEBUG_LINK_LIBRARIES asan ubsan)

FILE(GLOB OLOG_SOURCES *.cc)

add_library(olog ${OLOG_SOURCES}) # Release 版本
add_library(olog_debug ${OLOG_SOURCES}) # Debug 版本
add_library(olog_mt_debug ${OLOG_SOURCES}) # 多线程 Debug 版本

target_compile_options(olog PRIVATE ${OLOG_RELEASE_COMPILE_FLAGS})
target_compile_options(olog_debug PUBLIC ${OLOG_DEBUG_COMPILE_FLAGS})
target_compile_options(olog_mt_debug PRIVATE ${OLOG_MT_DEBUG_COMPILE_FLAGS})

# 启用调试输出信息。
target_compile_definitions(olog_debug PUBLIC OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING)
target_compile_definitions(olog_debug PUBLIC OLOG_ENABLE_LOGGER_DEBUG_PRINTTING)

target_link_libraries(olog ${OLOG_LINK_LIBRARIES})
target_link_libraries(olog_debug ${OLOG_DEBUG_LINK_LIBRARIES} ${OLOG_LINK_LIBRARIES} )
target_link_libraries(olog_mt_debug ${OLOG_DEBUG_LINK_LIBRARIES} ${OLOG_LINK_LIBRARIES} )