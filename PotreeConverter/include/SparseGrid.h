

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

class SparseGrid : public map<long long, GridCell*>{
public:
	int width;
	int height;
	int depth;
	AABB aabb;
	float squaredSpacing;
	int numAccepted;

	static long long count;

	SparseGrid(AABB aabb, float minGap);

	SparseGrid(const SparseGrid &other)
		: width(other.width), height(other.height), depth(other.depth), aabb(other.aabb), squaredSpacing(other.squaredSpacing), numAccepted(other.numAccepted)
	{
		count++;
	}

	~SparseGrid();

	bool isDistant(const Vector3<double> &p, GridCell *cell);

	bool add(Vector3<double> &p);

	void addWithoutCheck(Vector3<double> &p);

};



#endif