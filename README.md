# Potree Converter

## Changes

* Added support for las files.<br>
Old loader must be reimplemented, only las files can be converted at the moment.

## Usage

Converts las files to the potree file format.


    # pointDensity:		specifies the distance between points at root level. Each subsequent level has half the point density from the previous level
    # maxRecursions:	number of levels
    PotreeConverter <sourcePath> <targetDir> <pointDensity> <maxRecursions>
    
    # example
    PotreeConverter C:/pointclouds/pointcloud.las C:/pointclouds/converted 1.0 5
