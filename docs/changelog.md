## 2015.03.09

### features

* Splitted .bin files in folders.
* Introduced the concept of hierarchy and hierarchyStepSize
* 	hierarchyStepSize defines how many depth-levels are kept in the same directory (further levels are stored in subdirectories)
* 	hierarchy: The .bin files are organized in subdirectories inside ./data. The cloud.js contains only the hierarchy information of the root (which keeps this file small independently of the cloud size). The hierarchy is stored in .hrc files, in packets of 5 bytes. This list of 5-byte packets is a breadth-first traversal of the tree, starting in itself.
* 		1 byte with a mask of this node's children. E.g: 00000011 in the file r.hrc refers to the nodes r0 and r1
* 		4 bytes (unsigned long int) storing the number of points in that node.

For example, consider the following output of PotreeConverter:
*	./data/r
*	./data/r.bin
*	./data/r.hrc
*		./data/r/r0.bin
*		./data/r/r0.hrc
*		./data/r/r1.bin
*		./data/r/r1.hrc

The file ./cloud.js will contain information only about the root:
        "hierarchy": [
                ["r", 13291]
        ]

The file ./data/r.hrc will contain 2 packs of 5-bytes withthe following data:
* (3,$number_of_points_in_r.bin)
* (0,$number_of_points_in_r0.bin)
* (0,$number_of_points_in_r1.bin)

* The byte containing the value 3 (00000011 in binary) shows that the root contains the nodes r0 and r1. The tree is traversed breadth-first, so the following two lines correspond to r0 and r1, respectively:
* r0 has a value 0 for the mask (so no more children)
* r1 has a value 0 for the mask (so no more children)




## 2014.12.30

### features

* Updated BINARY format to version 1.4. Coordinates are now stored as integers, rather than floats. Additionaly, a tightBoundingBox is also saved in the cloud.js file. The normal boundingBox specifies the cubic octree bounds whereas the tightBoundingBox specifies the extent of the actual data.

## 2014.12.17

### features
* Instead of specifying ```-s arg```, spacing can now also be calculated from the diagonal by specifying ```-d arg```.
  spacing = diagonal / arg. This has the advantage that it scales automatically with the extent of the point cloud.
  Thanks to @chiccorusso for this feature.
* The default value for the spacing has been changed from ```-s 1``` to ```-d 250```, i.e. spacing = diagonal / 250.
  This seems to work fine for small, as well as large point clouds.
* Added ```--scale arg``` parameter. This parameter specifies the precision of point coordinates (currently only for 
  LAS and LAZ output formats). Previously, this was fixed to 0.01 which gives centimeter precision for 
  point clouds with metric coordinate units. 
  Thanks to @chiccorusso for this feature.
* Added classification, point source ID and return number to LAS and LAZ output.

### bugfixes
* Fixed adaptive point sizes. Sometimes, the point size calculation close to the border of 2 nodes failed. This was because the bounding box was written to the cloud.js in low precision.
  
