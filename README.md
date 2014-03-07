# Potree Converter

## Dependencies

* [liblas](http://www.liblas.org/)
* [boost](http://www.boost.org/)

## Build

Linux/MacOSX:

    mkdir build && cd build
    cmake ../
    make

Windows:

    mkdir build && cd build
    #32bit
	cmake -G "Visual Studio 11" -T "v110" -DBoost_USE_STATIC_LIBS=ON -DBOOST_ROOT=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_LIBRARYDIR% -DLIBLAS_INCLUDE_DIR=%LIBLAS_INCLUDE_DIR% -DLIBLAS_LIBRARY=%LIBLAS_LIBRARY_DIR%/liblas.lib  ..\

	#or 64bit
	cmake -G "Visual Studio 11 Win64" -T "v110" -DBoost_USE_STATIC_LIBS=ON -DBOOST_ROOT=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_LIBRARYDIR% -DLIBLAS_INCLUDE_DIR=%LIBLAS_INCLUDE_DIR% -DLIBLAS_LIBRARY=%LIBLAS_LIBRARY_DIR%/liblas.lib  ..\

## PotreeConverter Usage

Converts las or xyzrgba-binary files to the potree file format.


    # pointDensity:		specifies the distance between points at root level. Each subsequent level has half the point density from the previous level
    # maxRecursions:	number of levels
    PotreeConverter <sourcePath> <targetDir> <pointDensity> <maxRecursions>
    
    # example
    PotreeConverter C:/pointclouds/pointcloud.las C:/pointclouds/converted 1.0 5

## xyzrgb2bin usage

Converts xyz files with rgb data to xyzrgba-binary files.
data has to be in following format (<> necessary, [] optional, will be ignored)

    <x> <y> <z> <r> <g> <b> [a]

for example:

    1.2312 4.311 5.12312 100 125 210
    5.2312 4.311 5.12312 123 124 125

usage:

    # rgbMaxValue:		indicates the range of rgb data. this value is used to normalize color values.
    xyzrgb2bin <sourcePath> <targetPath> <rgbMaxValue>

    # example
    xyzrgb2bin C:/pointclouds/pointcloud.xyz C:/pointclouds/pointcloud.bin 255
