# Potree Converter

Builds a potree octree from las, laz or binary ply files.

## Downloads

* [Windows 64bit binary 2014.11.18 (newest)](http://potree.org/downloads/PotreeConverter/PotreeConverter_2014.11.18.zip)
* [Windows 64bit binary 2014.08.31](http://potree.org/downloads/PotreeConverter/PotreeConverter_2014.08.31.zip)

## Dependencies

* [libLAS](https://github.com/libLAS/libLAS)
* [LASzip](https://github.com/LASzip/LASzip)
* [boost](http://www.boost.org/)

## Build

Linux/MacOSX:

    mkdir build && cd build
    cmake ../
    make

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

Windows / Microsoft Visual Studio 2012:

    # make sure you've got these environment variables set with your directory structure
    set BOOST_ROOT=D:\dev\lib\boost_1_56_0
    set BOOST_LIBRARYDIR=D:\dev\lib\boost\x64
    set LIBLAS_INCLUDE_DIR=D:\dev\lib\libLAS\include
    set LIBLAS_LIBRARY_DIR=D:\dev\lib\libLAS\build\bin\Release

    mkdir build
    cd build

    # 32bit project
    cmake -G "Visual Studio 11" -T "v110" -DBoost_USE_STATIC_LIBS=ON -DBOOST_ROOT=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_LIBRARYDIR% -DLIBLAS_INCLUDE_DIR=%LIBLAS_INCLUDE_DIR% -DLIBLAS_LIBRARY=%LIBLAS_LIBRARY_DIR%/liblas.lib  ..\

    # or 64bit project
    cmake -G "Visual Studio 11 Win64" -T "v110" -DBoost_USE_STATIC_LIBS=ON -DBOOST_ROOT=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_LIBRARYDIR% -DLIBLAS_INCLUDE_DIR=%LIBLAS_INCLUDE_DIR% -DLIBLAS_LIBRARY=%LIBLAS_LIBRARY_DIR%/liblas.lib  ..\

## PotreeConverter Usage

Converts las files to the potree file format.
You can list multiple input files. If a directory is specified, all files
inside the directory will be converted.

Options:

    -h [ --help ]             prints usage
    -o [ --outdir ] arg       output directory
    -s [ --spacing ] arg      Distance between points at root level. Distance
                              halves each level.
    -l [ --levels ] arg       Number of levels that will be generated. 0: only
                              root, 1: root and its children, ...
    -f [ --input-format ] arg Input format. xyz: cartesian coordinates as floats,
                              rgb: colors as numbers, i: intensity as number
    -r [ --range ] arg        Range of rgb or intensity.
    --output-format arg       Output format can be BINARY, LAS or LAZ. Default is
                              BINARY
    --source arg              Source file. Can be LAS, LAZ or PLY

Examples:

    # convert data.las with a spacing of 0.5 and a depth of 4
    ./PotreeConverter.exe C:/data.las -s 0.5 -l 4 -o C:/potree_converted

    # same as before but output is LAZ compressed
    ./PotreeConverter.exe C:/data.las -s 0.5 -l 4 -o C:/potree_converted --output-format LAZ

    # convert data1.las and data2.las with a spacing of 0.5 and a depth of 4
    ./PotreeConverter.exe C:/data1.las C:/data1.las C:/data2.las -s 0.5 -l 4 -o C:/potree_converted

    # convert all files inside the data directory
    ./PotreeConverter.exe C:/data -s 0.5 -l 4 -o C:/potree_converted
