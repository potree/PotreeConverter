# Potree Converter

Builds a potree octree from las, laz, binary ply, xyz or ptx files.

## Downloads

* [PotreeConverter 1.2, windows 64bit](https://github.com/potree/PotreeConverter/releases/tag/1.2)
* [PotreeConverter 1.1.1, windows 64bit](http://potree.org/downloads/PotreeConverter/PotreeConverter_1.1.1.zip)
* [PotreeConverter 1.0, windows 64bit](http://potree.org/downloads/PotreeConverter/PotreeConverter_2014.12.30.zip)

## Changelog

See [docs/changelog.md](./docs/changelog.md) for a list of new features, bugfixes and changes to the API.

## Dependencies

* [libLAS](https://github.com/libLAS/libLAS)
* [LASzip](https://github.com/LASzip/LASzip)
* [boost](http://www.boost.org/)

## Build

```
# clone repository
git clone https://github.com/potree/PotreeConverter.git potree

# update submodules
git submodule init
git submodule update
```

Linux/MacOSX:

    mkdir build && cd build
    cmake ../
    make

    # copy ./PotreeConverter/resources/page_template to your binary working directory.

Linux with custom builds of liblas and laszip

```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DLIBLAS_INCLUDE_DIR=/opt/source/libLAS-1.8.0/build/include/ -DLIBLAS_LIBRARY=/opt/source/libLAS-1.8.0/build/lib/liblas.so -DLASZIP_INCLUDE_DIR=/opt/source/laszip-2.1.0/build/include -DLASZIP_LIBRARY=/opt/source/laszip-2.1.0/build/lib/liblaszip.so ..
```

Ubuntu:

    sudo apt-get install libboost-system-dev libboost-thread-dev

    # Add UbuntuGIS "unstable" PPA from Launchpad
    # (TODO: test if this PPA is really necessary)
    sudo add-apt-repository ppa:ubuntugis/ubuntugis-unstable
    sudo apt-get update
    sudo apt-get install liblas-dev liblas-c-dev

    mkdir build && cd build
    cmake ../
    make

    # copy ./PotreeConverter/resources/page_template to your binary working directory.

Windows / Microsoft Visual Studio 2015:

```
# make sure you've got these environment variables set with your directory structure
set BOOST_ROOT=D:\dev\lib\boost_1_58_0
set BOOST_LIBRARYDIR=D:\dev\lib\boost\1.58_x64_msvc2015
set LIBLAS_INCLUDE_DIR=D:\dev\lib\libLAS\include
set LIBLAS_LIBRARY_DIR=D:\dev\lib\libLAS\build\bin\Release

# compile boost
b2 toolset=msvc-14.0 address-model=64 link=static link=shared threading=multi --build-type=complete stage --width-system --with-thread --with-filesystem --with-program_options --with-regex

mkdir build
cd build

# or 64bit project
cmake -G "Visual Studio 14 2015 Win64" -DBoost_USE_STATIC_LIBS=ON -DBOOST_ROOT=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_LIBRARYDIR% -DLIBLAS_INCLUDE_DIR=%LIBLAS_INCLUDE_DIR% -DLIBLAS_LIBRARY=%LIBLAS_LIBRARY_DIR%/liblas.lib  ..\

# copy ./PotreeConverter/resources/page_template to your binary working directory.
```

## PotreeConverter Usage

Converts las files to the potree file format.
You can list multiple input files. If a directory is specified, all files
inside the directory will be converted.

Options:


```
-h [ --help ]                           prints usage
-p [ --generate-page ] arg              Generates a ready to use web page with the given name.
-o [ --outdir ] arg                     output directory
-s [ --spacing ] arg                    Distance between points at root level. Distance halves each level.
-d [ --spacing-by-diagonal-fraction ] arg
                                        Maximum number of points on the diagonal in the first level (sets spacing). spacing = diagonal / value
-l [ --levels ] arg                     Number of levels that will be generated. 0: only root, 1: root and its children, ...
-f [ --input-format ] arg               Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number
--color-range arg
--intensity-range arg
--output-format arg                     Output format can be BINARY, LAS or LAZ. Default is BINARY
-a [ --output-attributes ] arg          can be any combination of RGB, INTENSITY, CLASSIFICATION and NORMAL. Default is RGB.
--scale arg                             Scale of the X, Y, Z coordinate in LAS and LAZ files.
--aabb arg                              Bounding cube as "minX minY minZ maxX maxY maxZ". If not provided it is automatically computed
--incremental                           Add new points to existing conversion
--overwrite                             Replace existing conversion at target directory
--source arg                            Source file. Can be LAS, LAZ, PTX or PLY
```

Examples:

    # convert data.las and generate web page.
    ./PotreeConverter.exe C:/data.las -o C:/potree_converted -p data

    # generate compressed LAZ files instead of the default BIN format.
    ./PotreeConverter.exe C:/data.las -o C:/potree_converted --output-format LAZ

    # convert all files inside the data directory
    ./PotreeConverter.exe C:/data -o C:/potree_converted

    # first, convert with custom bounding box and then append new_data.las afterwards.
    # points in new_data MUST fit into bounding box!
    ./PotreeConverter.exe C:/data -o C:/potree_converted -aabb "-0.748 -2.780 2.547 3.899 1.867 7.195"
    ./PotreeConverter.exe C:/new_data.las -o C:/potree_converted --incremental
