
# About

PotreeConverter generates an octree LOD structure for streaming and real-time rendering of massive point clouds. The results can be viewed in web browsers with [Potree](https://github.com/potree/potree) or as a desktop application with [PotreeDesktop](https://github.com/potree/PotreeDesktop). 

Version 2.0 is a complete rewrite with following differences over the previous version 1.7:

* About 10 to 50 times faster than PotreeConverter 1.7 on SSDs.
* Produces a total of 3 files instead of thousands to tens of millions of files. The reduction of the number of files improves file system operations such as copy, delete and upload to servers from hours and days to seconds and minutes. 
* Better support for standard LAS attributes and arbitrary extra attributes. Full support (e.g. int64 and uint64) in development.
* Optional compression is not yet available in the new converter but on the roadmap for a future update.

Altough the converter made a major step to version 2.0, the format it produces is also supported by Potree 1.7. The Potree viewer is scheduled to make the major step to version 2.0 in 2021, with a rewrite in WebGPU. 

To fund the future development of this project, PotreeConverter 2.0 will be available for use under a free license for non-commercial and non-government entities, and under a paid license for commercial and government entities - see license and pricing section below. [PotreeConverter 1.7](https://github.com/potree/PotreeConverter/releases/tag/1.7) source and binaries continue to be available for free for anyone to use, as well as the alternative [Entwine](https://entwine.io/).

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

# Alternatives

PotreeConverter 2.0 is only free for non-commercial and non-government use. The following table lists free and open source alternatives if PotreeConveter 2.0 doesn't suit you. All of them produce LOD structures that can be loaded and rendered with Potree. 

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
			free for non-commercial
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

# License and Pricing

Summary: 
* Free for non-commercial and non-government use, including universities and research institutions as long as Potree is not used for or part of a commercial product or service.
* Paid license for commercial and government uses.
* If the licensing terms don't fit your use case (e.g. integration as an exporter in your software, art projects, ...),  please contact licensing@potree.org for special conditions and exceptions. 

See [LICENSE](LICENSE) for detailed information. 

## Pricing

You can evaluate PotreeConverter for free for three months before you need to purchase a license. Licenses are per simultaneously running instance of PotreeConverter 2.0. The licensee can install PotreeConverter on multiple devices of the licensee, but only keep one instance of the converter running at a time. 

1. 240€ per instance for a perpetual license of the software at the date of purchase, plus one year of updates. 
2. 1200€ per instance and year for fully automated SaaS hosting services that include PotreeConverter. This applies to online services that provide customers a platform to upload their data, which is then automatically processed by a converter instance, and finally made available online publicly or behind authentication (e.g. youtube or sketchfab for point clouds). 
3. Anyone who has donated to or funded Potree before August 2020 is considered to have a perpetual license under bullet point [1], plus additional numbers of [1] or [2] that match the amount of donation/funding. 

* Note that this only applies to the licensee using PotreeConverter 2.0. You're allowed to convert your customers data with PotreeConverter 2.0 and host it online via Potree 1.7(free), without the need to aquire additional licenses for customers. You're also allowed to distribute converted data via PotreeDesktop 1.7 without the need for additional licenses. The customer will only need to purchase licenses if they decide to convert point cloud data with the converter themselves. 
* If in doubt, just start using it for the three month trial period and ask for clarifications via licensing@potree.org
* Licensees will have priority support. I'll reply to smaller issues as fast as possible, and offer support plans for larger issues. 

Licenses can be purchased with following options:

* [Paypal](http://potree.org/licensing.html) (with volume licenses)
* Github Sponsorship, see "Sponsor" button at the top. (monthly single license)
* Via invoice and wire transfer - contact licensing@potree.org 
