

set(header_files
	arithmeticdecoder.hpp
	arithmeticencoder.hpp
	arithmeticmodel.hpp
	bytestreamin.hpp
	bytestreamin_array.hpp
	bytestreamin_file.hpp
	bytestreamin_istream.hpp
	bytestreaminout.hpp
	bytestreaminout_file.hpp
	bytestreamout.hpp
	bytestreamout_array.hpp
	bytestreamout_file.hpp
	bytestreamout_nil.hpp
	bytestreamout_ostream.hpp
	integercompressor.hpp
	lasattributer.hpp
	lasindex.hpp
	lasinterval.hpp
	laspoint.hpp
	lasquadtree.hpp
	lasquantizer.hpp
	lasreaditem.hpp
	lasreaditemcompressed_v1.hpp
	lasreaditemcompressed_v2.hpp
	lasreaditemcompressed_v3.hpp
	lasreaditemraw.hpp
	lasreadpoint.hpp
	lasunzipper.hpp
	laswriteitem.hpp
	laswriteitemcompressed_v1.hpp
	laswriteitemcompressed_v2.hpp
	laswriteitemcompressed_v3.hpp
	laswriteitemraw.hpp
	laswritepoint.hpp
	laszip.hpp
	laszip_common_v1.hpp
	laszip_common_v2.hpp
	laszip_common_v3.hpp
	laszip_decompress_selective_v3.hpp
	laszipper.hpp
	mydefs.hpp
)

set(source_files
	arithmeticdecoder.cpp
	arithmeticencoder.cpp
	arithmeticmodel.cpp
	integercompressor.cpp
	lasindex.cpp
	lasinterval.cpp
	lasquadtree.cpp
	lasreaditemcompressed_v1.cpp
	lasreaditemcompressed_v2.cpp
	lasreaditemcompressed_v3.cpp
	lasreadpoint.cpp
	lasunzipper.cpp
	laswriteitemcompressed_v1.cpp
	laswriteitemcompressed_v2.cpp
	laswriteitemcompressed_v3.cpp
	laswritepoint.cpp
	laszip.cpp
	laszip_dll.cpp
	laszipper.cpp
)


#if(WIN32)
	list(APPEND header_files ../dll/laszip_api.h)
	list(APPEND source_files laszip_dll.cpp)
#endif()

set(files ${header_files} ${source_files})

add_definitions("-DLASZIPDLL_EXPORTS")
add_definitions("-DUNORDERED")
if(WIN32)
	add_definitions("-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS")
	add_definitions("-D_CRT_SECURE_NO_WARNINGS")
	add_definitions("-D_CRT_NONSTDC_NO_DEPRECATE")
endif()

include_directories(. ../dll)

add_library(laszip STATIC ${files})

install(TARGETS laszip 
	RUNTIME DESTINATION bin/
	LIBRARY DESTINATION lib/
	ARCHIVE DESTINATION lib/
)













