cmake_minimum_required(VERSION 3.2)

project(real_poker_cpp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

find_package(Boost REQUIRED date_time system thread)

include_directories(${Boost_INCLUDE_DIR})

file(GLOB_RECURSE CFR_SRC src/*.cpp)
add_library(cfr_lib STATIC ${CFR_SRC})
target_include_directories(cfr_lib PUBLIC .)
target_include_directories(cfr_lib PUBLIC ../eval7pp)
target_include_directories(cfr_lib PUBLIC ../cpptqdm)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../eval7pp ${CMAKE_CURRENT_BINARY_DIR}/eval7pp)
add_executable(eval_cfr eval_cfr.cpp)
target_link_libraries(eval_cfr PRIVATE cfr_lib eval7pp)
target_include_directories(eval_cfr PRIVATE ../cpptqdm)

add_executable(multi_mccfr multi_mccfr.cpp)
target_link_libraries(multi_mccfr PUBLIC ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})
target_link_libraries(multi_mccfr PRIVATE pthread cfr_lib eval7pp)
target_include_directories(multi_mccfr PRIVATE ../cpptqdm)

add_executable(run_equity_calcs run_equity_calcs.cpp)
target_link_libraries(run_equity_calcs PUBLIC ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})
target_link_libraries(run_equity_calcs PRIVATE pthread cfr_lib eval7pp)
target_include_directories(run_equity_calcs PRIVATE ../cpptqdm)