project(PotreeConverter)

set(RAPIDJSON_INCLUDE_DIR lib/rapidjson/include)
set(ARGUMENTS_INCLUDE_DIR lib/arguments)

file(GLOB_RECURSE files src/*.cpp include/*.h include/*.hpp )
file(GLOB_RECURSE lib_rapidjson_files ${RAPIDJSON_INCLUDE_DIR}/* )
file(GLOB_RECURSE lib_argument_files ${ARGUMENTS_INCLUDE_DIR}/* )

include_directories(include ${LASZIP_INCLUDE_DIRS})
include_directories(include ${RAPIDJSON_INCLUDE_DIR})
include_directories(include ${ARGUMENTS_INCLUDE_DIR})

SOURCE_GROUP("Header Files\\io" FILES
	include/BoostBINPointReader.hpp
	include/BINPointReader.hpp
	include/BINPointWriter.hpp
	include/LASPointReader.h
	include/LASPointWriter.hpp
	include/PlyPointReader.h
	include/PointReader.h
	include/PointWriter.hpp
	include/PTXPointReader.h
	include/XYZPointReader.hpp
	include/FlatBufferReader.hpp
)

SOURCE_GROUP("Source Files\\io" FILES
	src/BoostBINPointReader.cpp
	src/BINPointReader.cpp
	src/LASPointReader.cpp
	src/LASPointWriter.cpp
	src/PTXPointReader.cpp
	src/FlatBufferReader.cpp
)

SOURCE_GROUP("lib\\rapidjson" FILES ${lib_rapidjson_files})
SOURCE_GROUP("lib\\arguments" FILES ${lib_argument_files})

add_executable(PotreeConverter ${files} ${lib_rapidjson_files} ${lib_argument_files} include/FlatBufferReader.hpp src/FlatBufferReader.cpp)

target_link_libraries(PotreeConverter ${LASZIP_LIBRARY})

# Find Boost for serialization
find_package(Boost COMPONENTS serialization REQUIRED)
target_link_libraries(PotreeConverter ${Boost_LIBRARIES})

# Find and include LASzip
find_package(LASzip REQUIRED)
target_include_directories(PotreeConverter PRIVATE ${LASzip_INCLUDE_DIRS})
target_link_libraries(PotreeConverter PRIVATE ${LASzip_LIBRARIES})

# Link against DataSchemas:
target_link_libraries(PotreeConverter DataSchemas)

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -pthread -lstdc++ -lm")
    target_link_libraries(PotreeConverter -lstdc++fs)
endif()

install(TARGETS PotreeConverter RUNTIME DESTINATION bin/ )
add_custom_command(TARGET PotreeConverter POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/PotreeConverter/PotreeConverter/resources $<TARGET_FILE_DIR:PotreeConverter>/resources)
