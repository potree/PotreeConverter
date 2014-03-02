cmake_minimum_required(VERSION 2.8)

project(Potree)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(LibLAS REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)

if(UNIX)
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  endif()
endif()

add_subdirectory(PotreeConverter)
add_subdirectory(xyzrgb2bin)
