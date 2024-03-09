cmake_minimum_required(VERSION 3.28)

project(OLog VERSION 0.0.1)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(OLOG_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src)
set(OLOG_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)