cmake_minimum_required(VERSION 3.16)
set(CMAKE_SUPPRESS_REGENERATION true)

project(LASzip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(ROOT_DIR "${PROJECT_SOURCE_DIR}")

# the next line is the ONLY place in the entire laszip system where
# the version info is hard-coded
set(LASZIP_API_VERSION_STRING "3.4.1" CACHE STRING "LASzip version" FORCE)
set(LASZIP_API_VERSION ${LASZIP_API_VERSION_MAJOR}.${LASZIP_API_VERSION_MINOR}.${LASZIP_API_VERSION_PATCH})

file(GLOB source_files
	"src/*.h"
	"src/*.hpp"
	"src/*.cpp"
	"dll/*.c"
	"dll/*.h"
)

list(FILTER source_files EXCLUDE REGEX ".*src/demzip_dll.cpp$")
list(FILTER source_files EXCLUDE REGEX ".*dll/demzip_api.c$")
list(FILTER source_files EXCLUDE REGEX ".*dll/demzip_api.h$")
list(FILTER source_files EXCLUDE REGEX ".*dll/laszip_api.h$")
list(FILTER source_files EXCLUDE REGEX ".*dll/laszip_api.c$")

#message("${source_files}") 


add_library(laszip SHARED ${source_files})
target_include_directories(laszip PRIVATE 
	./dll
	./src
)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

add_compile_definitions(OPENCV_VERSION=${OpenCV_VERSION})
add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
add_compile_definitions(_CRT_NONSTDC_NO_WARNING)
add_compile_definitions(_SCL_SECURE_NO_WARNINGS)
add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(LASZIP_DLL_EXPORT=1)
add_compile_definitions(LASZIPDLL_EXPORTS)
add_compile_definitions(UNORDERED)
add_compile_definitions(HAVE_UNORDERED_MAP=1)