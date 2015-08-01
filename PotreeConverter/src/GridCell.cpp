
#include "GridCell.h"
#include "SparseGrid.h"

#include <iostream>

using std::cout;
using std::endl;

using std::min;
using std::max;


GridCell::GridCell(){

}

GridCell::GridCell(SparseGrid *grid, GridIndex &index){
	this->grid = grid;
    neighbours.reserve(26);

	for(int i = max(index.i -1, 0); i <= min(grid->width-1, index.i + 1); i++){
		for(int j = max(index.j -1, 0); j <= min(grid->height-1, index.j + 1); j++){
			for(int k = max(index.k -1, 0); k <= min(grid->depth-1, index.k + 1); k++){

				long long key = ((long long)k << 40) | ((long long)j << 20) | i;
				SparseGrid::iterator it = grid->find(key);
				if(it != grid->end()){
					GridCell *neighbour = it->second;
					if(neighbour != this){
						neighbours.push_back(neighbour);
						neighbour->neighbours.push_back(this);
					}
				}

			}
		}
	}
}

float GridCell::minGap(const Vector3<double> &p){
	float minGap = MAX_FLOAT;

	for (const auto &point :  points){
		minGap = min(minGap, (float)point.distanceTo(p));
	}

	return minGap;
}

float GridCell::squaredMinGap(const Vector3<double> &p){
	float minGap = MAX_FLOAT;

	for (const auto &point :  points){
		minGap = min(minGap, (float)point.squaredDistanceTo(p));
	}

	return minGap;
}

float GridCell::minGapAtArea(const Vector3<double> &p){
	float areaGap = MAX_FLOAT;
	for (const auto &neighbour :  neighbours){
		areaGap = min(neighbour->minGap(p), areaGap);
	}

	return areaGap;
}

void GridCell::add(Vector3<double> p){
	points.push_back(p);
}

bool GridCell::isDistant(const Vector3<double> &p, const double squaredSpacing) const {
	for(const Vector3<double> &point : points){
		double dx = p.x - point.x;
		double dy = p.y - point.y;
		double dz = p.z - point.z;
		double squaredDistance = dx * dx + dy * dy + dz * dz;

		if(squaredDistance < squaredSpacing){
			return false;
		}
	}

	return true;
}
