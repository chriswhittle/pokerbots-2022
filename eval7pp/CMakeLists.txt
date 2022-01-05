cmake_minimum_required(VERSION 3.2)

project(eval7pp)

set(CMAKE_CXX_FLAGS "-O3") # we want eval7pp to be fast

file(GLOB_RECURSE EVAL7_SRC src/*.cpp)
add_library(eval7pp STATIC ${EVAL7_SRC})
target_include_directories(eval7pp PUBLIC .)
