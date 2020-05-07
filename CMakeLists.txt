cmake_minimum_required(VERSION 2.8)

project(Potree)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

SET(CMAKE_CXX_STANDARD 17)

add_subdirectory(PotreeConverter)
