cmake_minimum_required(VERSION 3.2)

project(cpp-pokerbot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "") #-O3)
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "")

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

add_subdirectory(libs)

# add_library(real_poker_cpp STATIC libs/real_poker_cpp/lib.cpp)
# target_include_directories(real_poker_cpp PUBLIC libs/real_poker_cpp)

file(GLOB_RECURSE BOT_SRC ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(pokerbot ${BOT_SRC})
target_include_directories(pokerbot PUBLIC ${PROJECT_SOURCE_DIR}/include)
# target_include_directories(pokerbot PUBLIC libs/eval7pp)
# target_include_directories(pokerbot PUBLIC libs/real_poker_cpp)
# target_include_directories(pokerbot PUBLIC libs/cpptqdm)
target_link_libraries(pokerbot skeleton cfr_lib eval7pp)
