

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
	float squaredSpacing;

	SparseGrid(AABB aabb, float minGap);

	~SparseGrid();

	vector<GridCell*> targetArea(int x, int y, int z);

	bool isDistant(const Vector3<double> &p, int i, int j, int k);

	bool add(Vector3<double> &p);

	void addWithoutCheck(Vector3<double> &p);

};



#endif