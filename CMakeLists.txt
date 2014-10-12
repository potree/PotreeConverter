cmake_minimum_required(VERSION 2.8.4)
project(PotreeConverter)

SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Boost 1.54 COMPONENTS system program_options filesystem regex REQUIRED)
find_package(LibLas 1.8.0 REQUIRED)

include_directories(
    PotreeConverter/include 
    ${LIBLAS_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    PotreeConverter/include/AABB.h
    PotreeConverter/include/BINPointReader.hpp
    PotreeConverter/include/BINPointWriter.hpp
    PotreeConverter/include/CloudJS.hpp
    PotreeConverter/include/definitions.hpp
    PotreeConverter/include/GridCell.h
    PotreeConverter/include/GridIndex.h
    PotreeConverter/include/LASPointReader.h
    PotreeConverter/include/LASPointWriter.hpp
    PotreeConverter/include/PlyPointReader.h
    PotreeConverter/include/Point.h
    PotreeConverter/include/PointAttributes.hpp
    PotreeConverter/include/PointReader.h
    PotreeConverter/include/PointWriter.hpp
    PotreeConverter/include/PotreeConverter.h
    PotreeConverter/include/PotreeException.h
    PotreeConverter/include/PotreeWriter.h
    PotreeConverter/include/SparseGrid.h
    PotreeConverter/include/stuff.h
    PotreeConverter/include/Vector3.h
    PotreeConverter/src/BINPointReader.cpp
    PotreeConverter/src/GridCell.cpp
    PotreeConverter/src/LASPointReader.cpp
    PotreeConverter/src/LASPointWriter.cpp
    PotreeConverter/src/main.cpp
    PotreeConverter/src/Point.cpp
    PotreeConverter/src/PointAttributes.cpp
    PotreeConverter/src/PotreeConverter.cpp
    PotreeConverter/src/PotreeWriter.cpp
    PotreeConverter/src/SparseGrid.cpp
    PotreeConverter/src/stuff.cpp
    PotreeConverter/src/Vector3.cpp)

add_executable(PotreeConverter ${SOURCE_FILES})

target_link_libraries(PotreeConverter
    ${Boost_LIBRARIES}
    ${LIBLAS_LIBRARY}
)
