# Potree Converter

Builds a potree octree from las, laz, binary ply, xyz or ptx files.

## NextDroid Branching Guidelines
`upstream-master` will remain the as the working upstream branch where we can pull changes down or push suggested changes up.

The remaining branches follow the typical NextDroid branching workflow, i.e.:
 - `master` is the stable release branch
 - `develop` is the pre-release branch
 - feature branches as needed

## Gotcha's
Converting csv files: 
 - file extension must be changed to `.xyz`
 
Formatting:
 - The following tokens specify the column type: 
   - `x`, `y`, `z` for x,y,z position coordinates
   - `X`, `Y`, `Z` for x,y,z values of normal vector
   - `r`, `g` `b` for r,g,b values 
   - `i` for intensity 
   - `t` for timestamp (not implemented yet)
   - 'c' for classification type (not implemented yet)
   - `s` to ignore (skip) column
 
 Example: for a file with the column order `[x, y, z, intensity, timestamp, class, unknown, r, g, b]` use the format flag: 
 ```
 -f xyzitcsrgb
 ```

## Dependencies

* [lastools(LASzip)](https://github.com/LAStools/LAStools) or [fork of lastools with cmake for LASzip](https://github.com/m-schuetz/LAStools)

## PotreeConverter Usage

Converts las files to the potree file format.
You can list multiple input files. If a directory is specified, all files
inside the directory will be converted.

Options:


```
$ PotreeConverter -h                                      
  -i [ --source ]                        input files
  -h [ --help ]                          prints usage
  -p [ --generate-page ]                 Generates a ready to use web page with the given name.
  -o [ --outdir ]                        output directory
  -s [ --spacing ]                       Distance between points at root level. Distance halves each level.
  -d [ --spacing-by-diagonal-fraction ]  Maximum number of points on the diagonal in the first level (sets spacing). spacing = diagonal value
  -l [ --levels ]                        Number of levels that will be generated. 0: only root, 1: root and its children, ...
  -f [ --input-format ]                  Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number
  --color-range
  --intensity-range
  --output-format                        Output format can be BINARY, LAS or LAZ. Default is BINARY
  -a [ --output-attributes ]             can be any combination of RGB, INTENSITY and CLASSIFICATION. Default is RGB.
  --scale                                Scale of the X, Y, Z coordinate in LAS and LAZ files.
  --aabb                                 Bounding cube as "minX minY minZ maxX maxY maxZ". If not provided it is automatically computed
  --incremental                          Add new points to existing conversion
  --overwrite                            Replace existing conversion at target directory
  --source-listing-only                  Create a sources.json but no octree.
  --projection                           Specify projection in proj4 format.
  --list-of-files                        A text file containing a list of files to be converted.
  --source                               Source file. Can be LAS, LAZ, PTX or PLY
  --title                                Page title
  --description                          Description to be shown in the page.
  --edl-enabled                          Enable Eye-Dome-Lighting.
  --show-skybox
  --material                             RGB, ELEVATION, INTENSITY, INTENSITY_GRADIENT, RETURN_NUMBER, SOURCE, LEVEL_OF_DETAIL
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
