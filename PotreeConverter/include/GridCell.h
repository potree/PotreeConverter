
#ifndef GRID_CELL_H
#define GRID_CELL_H

#include "Point.h"
#include "GridIndex.h"

#include <math.h>
#include <vector>

using std::min;
using std::max;
using std::vector;

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid;


class GridCell{
public:
	vector<Point> points;
	vector<GridCell*> neighbours;
	SparseGrid *grid;

	GridCell();

	GridCell(SparseGrid *grid, GridIndex &index);

	float minGap(const Point &p);

	void add(Point p);

	float minGapAtArea(const Point &p);
};

#endif