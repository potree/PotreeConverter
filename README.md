
# About

PotreeConverter generates an octree LOD structure for streaming and real-time rendering of massive point clouds. The results can be viewed in web browsers with [Potree](https://github.com/potree/potree) or as a desktop application with [PotreeDesktop](https://github.com/potree/PotreeDesktop). 

Version 2.0 is a complete rewrite with following differences over the previous version 1.7:

* About 10 to 50 times faster than PotreeConverter 1.7 on SSDs.
* Produces a total of 3 files instead of thousands to tens of millions of files. The reduction of the number of files improves file system operations such as copy, delete and upload to servers from hours and days to seconds and minutes. 
* Better support for standard LAS attributes and arbitrary extra attributes. Full support (e.g. int64 and uint64) in development.
* Optional compression is not yet available in the new converter but on the roadmap for a future update.

Altough the converter made a major step to version 2.0, the format it produces is also supported by Potree 1.7. The Potree viewer is scheduled to make the major step to version 2.0 in 2021, with a rewrite in WebGPU. 

# Publications

* [Potree: Rendering Large Point Clouds in Web Browsers](https://www.cg.tuwien.ac.at/research/publications/2016/SCHUETZ-2016-POT/SCHUETZ-2016-POT-thesis.pdf)
* [Fast Out-of-Core Octree Generation for Massive Point Clouds](https://www.cg.tuwien.ac.at/research/publications/2020/SCHUETZ-2020-MPC/), _Schütz M., Ohrhallinger S., Wimmer M._

# Getting Started

1. Download windows binaries or
    * Download source code
	* Install [CMake](https://cmake.org/) 3.16 or later
	* Create and jump into folder "build"
	    ```
	    mkdir build
	    cd build
	    ```
	* run 
	    ```
	    cmake ../
	    ```
	* On linux, run: ```make```
	* On windows, open Visual Studio 2019 Project ./Converter/Converter.sln and compile it in release mode
2. run ```PotreeConverter.exe <input> -o <outputDir>```
    * Optionally specify the sampling strategy:
	* Poisson-disk sampling (default): ```PotreeConverter.exe <input> -o <outputDir> -m poisson```
	* Random sampling: ```PotreeConverter.exe <input> -o <outputDir> -m random```

In Potree, modify one of the examples with following load command:

```javascript
let url = "../pointclouds/D/temp/test/metadata.json";
Potree.loadPointCloud(url).then(e => {
	let pointcloud = e.pointcloud;
	let material = pointcloud.material;

	material.activeAttributeName = "rgba";
	material.minSize = 2;
	material.pointSizeType = Potree.PointSizeType.ADAPTIVE;

	viewer.scene.addPointCloud(pointcloud);
	viewer.fitToScreen();
});

```
## PotreeConverter Usage
Converts las files to the potree file format. You can list multiple input files. If a directory is specified, all files inside the directory will be converted.
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
# Alternatives

PotreeConverter 2.0 produces a very different format than previous iterations. If you find issues, you can still try previous converters or alternatives:

<table>
	<tr>
		<th></th>
		<th>PotreeConverter 2.0</th>
		<th><a href="https://github.com/potree/PotreeConverter/releases/tag/1.7">PotreeConverter 1.7</a></th>
		<th><a href="https://entwine.io/">Entwine</a></th>
	</tr>
	<tr>
		<th>license</th>
		<td>
			free, BSD 2-clause
		</td>
		<td>
			free, BSD 2-clause
		</td>
		<td>
			free, LGPL
		</td>
	</tr>
	<tr>
		<th>#generated files</th>
		<td>
			3 files total
		</td>
		<td>
			1 per node
		</td>
		<td>
			1 per node
		</td>
	</tr>
	<tr>
		<th>compression</th>
		<td>
			none (TODO)
		</td>
		<td>
			LAZ (optional)
		</td>
		<td>
			LAZ
		</td>
	</tr>
</table>

Performance comparison (Ryzen 2700, NVMe SSD):

![](./docs/images/performance_chart.png)

# License 

PotreeConverter is available under the [BSD 2-clause license](./LICENSE).
