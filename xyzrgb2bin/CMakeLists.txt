project(xyzrgb2bin)

file(GLOB files src/*.cpp)

add_executable(xyzrgb2bin ${files})

install(TARGETS xyzrgb2bin
  RUNTIME DESTINATION bin/
)