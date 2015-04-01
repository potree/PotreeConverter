
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
				if(grid->find(GridIndex(i,j,k)) != grid->end()){
					GridCell *neighbour = (*grid)[GridIndex(i,j,k)];
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

	for(int i = 0; i < points.size(); i++){
		Vector3<double> point = points[i];
		minGap = min(minGap, (float)point.distanceTo(p));
	}

	return minGap;
}

float GridCell::squaredMinGap(const Vector3<double> &p){
	float minGap = MAX_FLOAT;

	for(int i = 0; i < points.size(); i++){
		Vector3<double> point = points[i];
		minGap = min(minGap, (float)point.squaredDistanceTo(p));
	}

	return minGap;
}

float GridCell::minGapAtArea(const Vector3<double> &p){
	float areaGap = MAX_FLOAT;
	for(int a = 0; a < neighbours.size(); a++){
		GridCell *neighbour = neighbours[a];
		float gap = neighbour->minGap(p);
		areaGap = min(gap, areaGap);
	}

	return areaGap;
}

void GridCell::add(Vector3<double> p){
	points.push_back(p);
}