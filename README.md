# Potree Converter

Builds a potree octree from las, laz, binary ply, xyz or ptx files.

## Downloads

* [PotreeConverter 1.1, windows 64bit](http://potree.org/downloads/PotreeConverter/PotreeConverter_1.1.zip)
* [PotreeConverter 1.0, windows 64bit](http://potree.org/downloads/PotreeConverter/PotreeConverter_2014.12.30.zip)

## Changelog

See [docs/changelog.md](./docs/changelog.md) for a list of new features, bugfixes and changes to the API.

## Dependencies

* [libLAS](https://github.com/libLAS/libLAS)
* [LASzip](https://github.com/LASzip/LASzip)
* [boost](http://www.boost.org/)

## Build

Linux/MacOSX:

    mkdir build && cd build
    cmake ../
    make

    # copy ./PotreeConverter/resources/page_template to your binary working directory.

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

    # copy ./PotreeConverter/resources/page_template to your binary working directory.

## PotreeConverter Usage

Converts las files to the potree file format.
You can list multiple input files. If a directory is specified, all files
inside the directory will be converted.

Options:


```
-h [ --help ]                         prints usage
-p [ --generate-page ]                Generates a ready to use web page alongwith the model.
-o [ --outdir ] arg                   output directory
-s [ --spacing ] arg                  Distance between points at root level. Distance halves each level.
-d [ --spacing-by-diagonal-fraction ] arg Maximum number of points on the diagonal in the first level (sets spacing). spacing = diagonal / value
-l [ --levels ] arg                   Number of levels that will be generated. 0: only root, 1: root and its children, ...
-f [ --input-format ] arg             Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number
--color-range arg
--intensity-range arg
--output-format arg                   Output format can be BINARY, LAS or
                                      LAZ. Default is BINARY
-a [ --output-attributes ] arg        Can be any combination of RGB, INTENSITY and CLASSIFICATION. Default is RGB.
--scale arg                           Scale of the X, Y, Z coordinate in LAS and LAZ files.
--source arg                          Source file. Can be LAS, LAZ, PTX or
```

Examples:

    # convert data.las and generate web page.
    ./PotreeConverter.exe C:/data.las -o C:/potree_converted -p

    # convert with an octree depth of 5 (-l); generate web page (-p); write RGB, INTENSITY and CLASSIFICATION attributes (-a)
    ./PotreeConverter.exe data.las -o C:/potree_converted -p -l 5 -a RGB INTENSITY CLASSIFICATION

    # generate compressed LAZ files instead of the default BIN format.
    ./PotreeConverter.exe C:/data.las -l 4 -o C:/potree_converted --output-format LAZ

    # convert data1.las and data2.las with a spacing of 0.5 and a depth of 4
    ./PotreeConverter.exe C:/data1.las C:/data1.las C:/data2.las -s 0.5 -l 4 -o C:/potree_converted

    # convert all files inside the data directory
    ./PotreeConverter.exe C:/data -s 0.5 -l 4 -o C:/potree_converted
