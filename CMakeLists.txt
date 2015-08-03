cmake_minimum_required(VERSION 3.3)

project(Potree)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(LibLAS REQUIRED)
find_package(Boost COMPONENTS system thread filesystem program_options regex REQUIRED)

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare -std=c++11 -pthread -lstdc++ -lm")
endif()

add_subdirectory(PotreeConverter)

message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")

