cmake_minimum_required(VERSION 2.8)
project(ARTSynchronized)

if( NOT CMAKE_BUILD_TYPE )
    message(STATUS "No build type selected, default to Release")
    set( CMAKE_BUILD_TYPE Release)
else()
    message(STATUS "Build type is set to ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -g -Wall -Wextra -march=native")

find_library(JemallocLib jemalloc)
find_library(TbbLib tbb)
find_package (Threads)
find_package (OpenMP REQUIRED)

set(ART_FILES OptimisticLockCoupling/Tree.cpp ROWEX/Tree.cpp ART/Tree.cpp)
add_library(ARTSynchronized ${ART_FILES})
target_link_libraries(ARTSynchronized ${TbbLib} ${JemallocLib} ${CMAKE_THREAD_LIBS_INIT} OpenMP::OpenMP_CXX) 
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g")

set(SOURCE_FILES example.cpp)
add_executable(example ${SOURCE_FILES})
target_link_libraries(example ARTSynchronized)
