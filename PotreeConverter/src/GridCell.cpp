
#include "GridCell.h"
#include "SparseGrid.h"

#include <iostream>

using std::cout;
using std::endl;


GridCell::GridCell(){

}

GridCell::GridCell(SparseGrid *grid, GridIndex &index){
	this->grid = grid;

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

float GridCell::minGap(const Point &p){
	//cout << "GridCell::minGapAtLevel(" << p << ", " << level << ")" << endl;
	float minGap = MAX_FLOAT;

	for(int i = 0; i < points.size(); i++){
		Point point = points[i];
		minGap = min(minGap, point.distanceTo(p));
	}

	return minGap;
}

float GridCell::minGapAtArea(const Point &p){
	float areaGap = MAX_FLOAT;
	for(int a = 0; a < neighbours.size(); a++){
		GridCell *neighbour = neighbours[a];
		float gap = neighbour->minGap(p);
		areaGap = min(gap, areaGap);
	}

	return areaGap;
}

void GridCell::add(Point p){
	points.push_back(p);
}