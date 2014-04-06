# Potree Converter

The PotreeConverter can transform following file formats into the potree format:

* __las__ files with XYZ and 2-byte RGB data.
* __ply__ files with "element vertex" as the first element and x,y,z,r,g,b properties. Subsequent elements will be ignored. xyz must be floats and r,g,b must be uchars. ASCII and binary_little_endian formats are supported.
* __xyz__ files with either xyzrgb or xyzi data. Specify the type, as well as the range of the data with the -f and -r options.

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

Converts las, xyz or ply files to the potree file format.

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
    --source arg              Source file. Can be LAS, PLY or XYZ
    
Examples:

    # convert ply files
    ./PotreeConverter.exe C:/data.ply -s 0.5 -l 4
    
    # convert las files
    ./PotreeConverter.exe C:/data.las -s 0.5 -l 4 -o C:/potree_converted
    
    # convert files in the xyzrgb format with rgb between 0 and 255
    ./PotreeConverter.exe C:/data.xyz -f xyzrgb -r 255
    
    # convert files in the xyzi format with intensity between 0 and 65536
    ./PotreeConverter.exe C:/data.xyz -f xyzi -r 65536

For example, the stanford bunny data in xyz format looks like this:

    -0.0378297 0.12794 0.00447467 0.850855 0.5 
    -0.0447794 0.128887 0.00190497 0.900159 0.5 
    -0.0680095 0.151244 0.0371953 0.398443 0.5 
    -0.00228741 0.13015 0.0232201 0.85268 0.5 	

* Columns 1-3: xyz
* Column 4: intensity in range 0-1
* Column 5: I don't know, will be ignored

xyz values are relatively small, therefore the spacing(-s) has to be small as well.

The command to convert this file into the potree format is:

    ./PotreeConverter.exe C:/bunny.xyz -f xyzi -r 1 -s 0.02
