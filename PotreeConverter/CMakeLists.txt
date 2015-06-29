project(PotreeConverter)

file(GLOB files src/*.cpp include/*.h include/*.hpp )

include_directories(include ${LIBLAS_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})

add_definitions(-D BOOST_NO_SCOPED_ENUMS)
add_definitions(-D BOOST_NO_CXX11_SCOPED_ENUMS)

add_executable(PotreeConverter ${files})
target_link_libraries(PotreeConverter ${LIBLAS_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY})

install(TARGETS PotreeConverter
  RUNTIME DESTINATION bin/
)
