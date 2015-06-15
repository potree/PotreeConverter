
#include <iostream>
#include <math.h>

#include "SparseGrid.h"

using std::min;

long long SparseGrid::count = 0;

SparseGrid::SparseGrid(AABB aabb, float spacing){
	count++;

	this->aabb = aabb;
	this->width =	(int)(aabb.size.x / (spacing * 5.0) );
	this->height =	(int)(aabb.size.y / (spacing * 5.0) );
	this->depth =	(int)(aabb.size.z / (spacing * 5.0) );
	this->squaredSpacing = spacing * spacing;
	numAccepted = 0;
}

SparseGrid::~SparseGrid(){
	count--;

	SparseGrid::iterator it;
	for(it = begin(); it != end(); it++){
		delete it->second;
	}
}


bool SparseGrid::isDistant(const Vector3<double> &p, GridCell *cell){
	if(!cell->isDistant(p, squaredSpacing)){
		return false;
	}

	for(int a = 0; a < cell->neighbours.size(); a++){
		GridCell *neighbour = cell->neighbours[a];
		if(!neighbour->isDistant(p, squaredSpacing)){
			return false;
		}
	}

	return true;


	//if(cell->squaredMinGap(p) < squaredSpacing){
	//	return false;
	//}
	//for(int a = 0; a < cell->neighbours.size(); a++){
	//	GridCell *neighbour = cell->neighbours[a];
	//	float gap = neighbour->squaredMinGap(p);
	//	if(gap < squaredSpacing){
	//		return false;
	//	}
	//}
	//
	//return true;
}

bool SparseGrid::add(Vector3<double> &p){
	int nx = (int)(width*(p.x - aabb.min.x) / aabb.size.x);
	int ny = (int)(height*(p.y - aabb.min.y) / aabb.size.y);
	int nz = (int)(depth*(p.z - aabb.min.z) / aabb.size.z);

	int i = min(nx, width-1);
	int j = min(ny, height-1);
	int k = min(nz, depth-1);

	GridIndex index(i,j,k);
	long long key = ((long long)k << 40) | ((long long)j << 20) | (long long)i;
	SparseGrid::iterator it = find(key);
	if(it == end()){
		it = this->insert(value_type(key, new GridCell(this, index))).first;
	}

	if(isDistant(p, it->second)){
		this->operator[](key)->add(p);
		numAccepted++;
		return true;
	}else{
		return false;
	}
}

void SparseGrid::addWithoutCheck(Vector3<double> &p){
	int nx = (int)(width*(p.x - aabb.min.x) / aabb.size.x);
	int ny = (int)(height*(p.y - aabb.min.y) / aabb.size.y);
	int nz = (int)(depth*(p.z - aabb.min.z) / aabb.size.z);

	int i = min(nx, width-1);
	int j = min(ny, height-1);
	int k = min(nz, depth-1);

	GridIndex index(i,j,k);
	long long key = ((long long)k << 40) | ((long long)j << 20) | (long long)i;
	SparseGrid::iterator it = find(key);
	if(it == end()){
		it = this->insert(value_type(key, new GridCell(this, index))).first;
	}

	it->second->add(p);
}