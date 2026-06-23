cmake_minimum_required(VERSION 3.16)
set(CMAKE_SUPPRESS_REGENERATION true)

project(PotreeConverter
	VERSION 2.0
	HOMEPAGE_URL "http://potree.org/"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

#message(${PROJECT_SOURCE_DIR})

set(HEADER_FILES  
	./Converter/include/Attributes.h
	./Converter/include/chunker_countsort_laszip.h
	./Converter/include/ChunkRefiner.h
	./Converter/include/ConcurrentWriter.h
	./Converter/include/converter_utils.h
	./Converter/include/indexer.h
	./Converter/include/prototyping.h
	./Converter/include/sampler_poisson.h
	./Converter/include/sampler_poisson_average.h
	./Converter/include/sampler_random.h
	./Converter/include/structures.h
	./Converter/include/Vector3.h
	./Converter/include/PotreeConverter.h
	./Converter/include/logger.h
	./Converter/modules/LasLoader/LasLoader.h
	./Converter/modules/unsuck/unsuck.hpp
)

add_executable(PotreeConverter 
	./Converter/src/chunker_countsort_laszip.cpp
	./Converter/src/indexer.cpp 
	./Converter/src/main.cpp
	./Converter/src/logger.cpp
	./Converter/modules/LasLoader/LasLoader.cpp
	./Converter/modules/unsuck/unsuck_platform_specific.cpp
	${HEADER_FILES}
)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif()

set(LASZIP_DIR "${PROJECT_SOURCE_DIR}/Converter/libs/laszip")
add_subdirectory(${LASZIP_DIR})
target_link_libraries(PotreeConverter laszip)

set(BROTLI_DIR "${PROJECT_SOURCE_DIR}/Converter/libs/brotli")
add_subdirectory(${BROTLI_DIR})
target_link_libraries(PotreeConverter brotlienc-static)
target_link_libraries(PotreeConverter brotlidec-static)


target_include_directories(PotreeConverter PRIVATE "./Converter/include")
target_include_directories(PotreeConverter PRIVATE "./Converter/modules")
target_include_directories(PotreeConverter PRIVATE "./Converter/libs")

include(CheckCXXSymbolExists)
set(_quiet_flag ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET TRUE)
check_cxx_symbol_exists(_LIBCPP_VERSION "version" LIBCPP)
check_cxx_symbol_exists(__GLIBCXX__ "version" GLIBCXX)
set(CMAKE_REQUIRED_QUIET ${_quiet_flag})
unset(_quiet_flag)

if (LIBCPP) # libc++
	message(WARNING "libc++ parallel STL is still in working. See https://libcxx.llvm.org/Status/PSTL.html")
	target_compile_options(${PROJECT_NAME} PRIVATE "-fexperimental-library")
	target_link_options(${PROJECT_NAME} PRIVATE "-fexperimental-library")
elseif (GLIBCXX) # libstdc++
	message(WARNING "TBB is used as backend of parallel algorithm in c++17. See https://gcc.gnu.org/onlinedocs/libstdc++/manual/parallel_mode.html")
	# The libstdc++ parallel mode is an experimental parallel implementation of many algorithms of the C++ Standard Library.
	# OpenMP or TBB can be used as a backend.
	find_package(TBB REQUIRED)
	target_link_libraries(${PROJECT_NAME} TBB::tbb)
endif()

###############################################
# COPY PAGE TEMPLATE TO BINARY DIRECTORY
###############################################

add_custom_command(
	TARGET PotreeConverter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${PROJECT_SOURCE_DIR}/resources/page_template
		$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/page_template)

###############################################
# COPY LICENSE FILES TO BINARY DIRECTORY
###############################################
add_custom_command(
	TARGET PotreeConverter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${LASZIP_DIR}/COPYING
		$<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses/license_laszip.txt)

add_custom_command(
	TARGET PotreeConverter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${BROTLI_DIR}/LICENSE
		$<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses/license_brotli.txt)

add_custom_command(
	TARGET PotreeConverter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${PROJECT_SOURCE_DIR}/Converter/libs/json/LICENSE
		$<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses/license_json.txt)

add_custom_command(
	TARGET PotreeConverter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${PROJECT_SOURCE_DIR}/LICENSE
		$<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses/license_potree_converter.txt)

add_custom_command(
	TARGET PotreeConverter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${PROJECT_SOURCE_DIR}/README.md
		$<TARGET_FILE_DIR:${PROJECT_NAME}>/README.md)


include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
	RENAME license_potree_converter.txt
	DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses
)

install(FILES ${LASZIP_DIR}/COPYING
	RENAME license_laszip.txt
	DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses
)
install(FILES ${BROTLI_DIR}/LICENSE
	RENAME license_brotli.txt
	DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses
)
install(FILES ${PROJECT_SOURCE_DIR}/Converter/libs/json/LICENSE
	RENAME license_json.txt
	DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses
)

include(CPack)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/licenses/license_potree_converter.txt")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
