

#ifndef TILING_GRID_H
#define TILING_GRID_H

#include "AABB.h"
#include "Point.h"
#include "GridIndex.h"
#include "GridCell.h"
#include "LASPointWriter.hpp"

#include <map>
#include <vector>
#include <math.h>

using std::vector;
using std::map;
using std::min;
using std::max;

#define MAX_FLOAT std::numeric_limits<float>::max()

class TilingGrid : public map<GridIndex, LASPointWriter*>{
public:
	float spacing;
	int numAccepted;
	double scale;
	string tilesDir;

	TilingGrid(float spacing, double scale, string tilesDir);

	TilingGrid(const TilingGrid &other)
		: spacing(other.spacing), numAccepted(other.numAccepted), tilesDir(other.tilesDir) {}

	~TilingGrid();

	void add(Point &point);

	void flush();
};



#endif