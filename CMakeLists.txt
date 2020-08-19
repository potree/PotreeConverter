cmake_minimum_required(VERSION 3.18)
set(CMAKE_SUPPRESS_REGENERATION true)

project(PotreeConverter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

add_subdirectory("${PROJECT_SOURCE_DIR}/Converter/libs/laszip")

message(${PROJECT_SOURCE_DIR})

set(HEADER_FILES  
	Converter/include/Attributes.h
	Converter/include/chunker_countsort_laszip.h
	Converter/include/ChunkRefiner.h
	Converter/include/ConcurrentWriter.h
	Converter/include/converter_utils.h
	Converter/include/indexer.h
	Converter/include/indexer_poissondisk.h
	Converter/include/indexer_random.h
	Converter/include/prototyping.h
	Converter/include/sampler_poisson.h
	Converter/include/sampler_poisson_average.h
	Converter/include/sampler_random.h
	Converter/include/structures.h
	Converter/include/Vector3.h
)

add_executable(PotreeConverter 
	./Converter/src/chunker_countsort_laszip.cpp
	./Converter/src/indexer.cpp 
	./Converter/src/indexer_poissondisk.cpp 
	./Converter/src/indexer_random.cpp 
	./Converter/src/main.cpp
	./Converter/modules/unsuck/unsuck_platform_specific.cpp
	${HEADER_FILES}
)

target_link_libraries(PotreeConverter laszip)

target_include_directories(PotreeConverter PRIVATE "./Converter/include")
target_include_directories(PotreeConverter PRIVATE "./Converter/modules")
target_include_directories(PotreeConverter PRIVATE "./Converter/libs")
