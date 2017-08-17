.PHONY: default
default: build

.PHONY: LAStools
LAStools:
	git submodule update --checkout
	mkdir -p LAStools/LASzip/build && \
	cd LAStools/LASzip/build && \
	cmake --verbose -DCMAKE_BUILD_TYPE=Release ../src && \
	make VERBOSE=1 -j$(nproc)

.PHONY: build
build: LAStools
	mkdir -p build && cd build && \
	cmake .. \
		-DCMAKE_BUILD_TYPE=Release \
		-DLASZIP_INCLUDE_DIRS=../LAStools/LASzip/dll \
		-DLASZIP_LIBRARY=$(PWD)/../PotreeConverter/LAStools/LASzip/build/src/liblaszip.so && \
	make -j$(nproc)

.PHONY: clean
clean:
	rm -rf LAStools build
