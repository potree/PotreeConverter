# Potree Converter

Builds a potree octree from las, laz, binary ply, xyz or ptx files.

## Downloads

* [Source Code and windows 64bit releases](https://github.com/potree/PotreeConverter/releases)
* [64bit VS2015 redistributable - necessary to exexute PotreeConverter!](https://www.microsoft.com/en-US/download/details.aspx?id=48145)

## Dependencies

* [lastools(LASzip)](https://github.com/LAStools/LAStools) or [fork of lastools with cmake for LASzip](https://github.com/m-schuetz/LAStools)
* [boost](http://www.boost.org/)

## Build

### linux / gcc 4.9


lastools (from fork with cmake)
```
cd ~/dev/workspaces/lastools
git clone https://github.com/m-schuetz/LAStools.git master
cd master/LASzip
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

```

PotreeConverter

```
cd ~/dev/workspaces/PotreeConverter
git clone https://github.com/potree/PotreeConverter.git master
cd master
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DLASZIP_INCLUDE_DIRS=~/dev/workspaces/lastools/master/LASzip/dll -DLASZIP_LIBRARY=~/dev/workspaces/lastools/master/LASzip/build/src/liblaszip.so ..
make

# copy ./PotreeConverter/resources/page_template to your binary working directory.

```

### Windows / Microsoft Visual Studio 2015:

lastools

```
cd D:/dev/workspaces/lastools/
git clone https://github.com/m-schuetz/LAStools.git master
cd master
mkdir build
cd build
cmake -G "Visual Studio 14 2015 Win64" ../
```

PotreeConverter

```
# make sure you've got these environment variables set with your directory structure
set LASZIP_INCLUDE_DIRS=D:\dev\workspaces\lastools\master\LASzip\dll
set LASZIP_LIBRARY=D:\dev\workspaces\lastools\master\LASzip\build\src\Release\laszip.lib
set BOOST_ROOT=D:\dev\lib\boost_1_58_0
set BOOST_LIBRARYDIR=D:\dev\lib\boost\1.58_x64_msvc2015

# compile boost
cd boostDir
b2 toolset=msvc-14.0 address-model=64 link=static link=shared threading=multi --build-type=complete stage --width-system --with-thread --with-filesystem --with-program_options --with-regex

# checkout PotreeConverter
cd D:/dev/workspaces/PotreeConverter
git clone https://github.com/potree/PotreeConverter.git master
cd master
mkdir build
cd build

# VS2015 64bit project
cmake -G "Visual Studio 14 2015 Win64" -DBoost_USE_STATIC_LIBS=ON -DBOOST_ROOT=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_LIBRARYDIR% -DLASZIP_INCLUDE_DIRS=%LASZIP_INCLUDE_DIRS% -DLASZIP_LIBRARY=%LASZIP_LIBRARY%  ..\

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
    ./PotreeConverter.exe C:/data.las -o C:/potree_converted -p pageName

    # generate compressed LAZ files instead of the default BIN format.
    ./PotreeConverter.exe C:/data.las -o C:/potree_converted --output-format LAZ

    # convert all files inside the data directory
    ./PotreeConverter.exe C:/data -o C:/potree_converted

    # first, convert with custom bounding box and then append new_data.las afterwards.
    # points in new_data MUST fit into bounding box!
    ./PotreeConverter.exe C:/data -o C:/potree_converted -aabb "-0.748 -2.780 2.547 3.899 1.867 7.195"
    ./PotreeConverter.exe C:/new_data.las -o C:/potree_converted --incremental
	
	# tell the converter that coordinates are in a UTM zone 10N projection. Also, create output in LAZ format
	./PotreeConverter.exe C:/data -o C:/potree_converted -p pageName --projection "+proj=utm +zone=10 +ellps=GRS80 +datum=NAD83 +units=m +no_defs" --overwrite --output-format LAZ
	
	# using a swiss projection. Use http://spatialreference.org/ to find projections in proj4 format
	./PotreeConverter.exe C:/data -o C:/potree_converted -p pageName --projection "+proj=somerc +lat_0=46.95240555555556 +lon_0=7.439583333333333 +k_0=1 +x_0=600000 +y_0=200000 +ellps=bessel +towgs84=674.4,15.1,405.3,0,0,0,0 +units=m +no_defs" --overwrite
	
	
	
