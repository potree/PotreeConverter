# Potree Converter

Builds a potree octree from las or laz files.

## Dependencies

* [laslib](http://rapidlasso.com/lastools/)
* [boost](http://www.boost.org/)

## Build

At the moment, there are only Visual C++ project files for windows.
Download and build the required dependencies and update the Visual Studio
Project paths.
You can find infos on how to build laslib dlls [here](https://groups.google.com/forum/#!msg/lastools/zDfLAcbSR7o/XAFdu1Nvie4J) 
and [here](https://groups.google.com/forum/#!topic/lastools/Bo4CaAMZIGk).

## PotreeConverter Usage

Converts las, xyz or ply files to the potree file format.
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
    --source arg              Source file. Can be las or laz.

Examples:

    # convert data.las with a spacing of 0.5 and a depth of 4
    ./PotreeConverter.exe C:/data.las -s 0.5 -l 4 -o C:/potree_converted

    # convert data1.las and data2.las with a spacing of 0.5 and a depth of 4
    ./PotreeConverter.exe C:/data1.las C:/data1.las C:/data2.las -s 0.5 -l 4 -o C:/potree_converted

    # convert all files inside the data directory
    ./PotreeConverter.exe C:/data C:/data2.las -s 0.5 -l 4 -o C:/potree_converted
