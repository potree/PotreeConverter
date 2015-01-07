
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
  
