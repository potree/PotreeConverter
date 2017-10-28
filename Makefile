.PHONY: default
default: build

.PHONY: LAStools
LAStools:
	[ -d $@ ] || git clone https://github.com/m-schuetz/LAStools.git
	cd LAStools/LASzip && \
	mkdir -p build && cd build && \
	cmake -DCMAKE_BUILD_TYPE=Release .. && \
	make -j$(nproc) && \
	cd ../../LASlib/src && make && \
	cd ../../src && \
	LIBS="-llaszip -L../LASzip/build/src" make

.PHONY: build

HERE=$(realpath ./)
build: LAStools
	mkdir -p build && cd build && \
	cmake .. \
		-DCMAKE_BUILD_TYPE=Release \
		-DLASZIP_INCLUDE_DIRS=$(HERE)/LAStools/LASzip/dll \
		-DLASZIP_LIBRARY_DIR=$(HERE)/LAStools/LASzip/build/src/ && \
	make -j$(nproc)

#		-DLASZIP_LIBRARY=$(HERE)/LAStools/LASzip/build/src/liblaszip.dylib && \

.PHONY: clean
clean:
	rm -rf LAStools build
