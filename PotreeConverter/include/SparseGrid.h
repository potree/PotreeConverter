

#ifndef SPARSE_GRID_H
#define SPARSE_GRID_H

#include "AABB.h"
#include "Point.h"
#include "GridIndex.h"
#include "GridCell.h"

#include <map>
#include <vector>
#include <math.h>

using std::vector;
using std::map;
using std::min;
using std::max;

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid : public map<GridIndex, GridCell*>{
public:
	int width;
	int height;
	int depth;
	float minGap;
	AABB aabb;

	SparseGrid(AABB aabb, float minGap);

	~SparseGrid();

	vector<GridCell*> targetArea(int x, int y, int z);

	float minGapAtTargetArea(const Point &p, int i, int j, int k);

	bool isDistant(const Point &p, int i, int j, int k);

	/**
	 * @return true if the point has been added to the grid 
	 */
	bool add(Point p, float &oMinGap);

	bool add(Point &p);

	void addWithoutCheck(Point &p);

};



#endif