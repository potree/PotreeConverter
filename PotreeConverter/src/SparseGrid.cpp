
#include "SparseGrid.h"


SparseGrid::SparseGrid(AABB aabb, float minGap){
	this->aabb = aabb;
	this->width = aabb.size.x / minGap;
	this->height = aabb.size.y / minGap;
	this->depth = aabb.size.z / minGap;
	this->minGap = minGap;
}

SparseGrid::~SparseGrid(){
	SparseGrid::iterator it;
	for(it = begin(); it != end(); it++){
		delete it->second;
	}
}


/**
 * returns target and neighbour cells
 */
vector<GridCell*> SparseGrid::targetArea(int x, int y, int z){
	vector<GridCell*> cells;

	for(int i = x - 1; i <= x + 1; i++){
		for(int j = y - 1; j <= y + 1; j++){
			for(int k = z - 1; k <= z + 1; k++){
				GridIndex index(i,j,k);
				if(find(index) != end()){
					cells.push_back(this->operator[](index));
				}
			}
		}
	}

	return cells;
}

float SparseGrid::minGapAtTargetArea(const Point &p, int i, int j, int k){
	//cout << "SparseGrid::minGapAtTargetArea(" << p << ", " << i << ", " << j << ", " << k << ", " << level << ")" << endl;
	//vector<GridCell> targetArea = this->targetArea(i,j,k);
	float minGap = MAX_FLOAT;

	for(int a = i - 1; a <= i + 1; a++){
		for(int b = j - 1; b <= j + 1; b++){
			for(int c = k - 1; c <= k + 1; c++){
				GridIndex index(a,b,c);
				if(find(index) != end()){
					GridCell *cell = this->operator[](index);
					float gap = cell->minGap(p);
					minGap = min(minGap, gap);
				}
			}
		}
	}

	//cout << "returning " << minGap << endl;
	return minGap;
}

bool SparseGrid::isDistant(const Point &p, int i, int j, int k){
	GridCell *cell = this->operator[](GridIndex(i,j,k));
	if(cell->minGap(p) < minGap){
		return false;
	}
	for(int a = 0; a < cell->neighbours.size(); a++){
		GridCell *neighbour = cell->neighbours[a];
		float gap = neighbour->minGap(p);
		if(gap < minGap){
			return false;
		}
	}

	return true;
}

bool SparseGrid::add(Point p, float &oMinGap){
	//cout << "SparseGrid::add(" << p << ")" << endl;
	int nx = (int)(width*(p.x - aabb.min.x) / aabb.size.x);
	int ny = (int)(height*(p.y - aabb.min.y) / aabb.size.y);
	int nz = (int)(depth*(p.z - aabb.min.z) / aabb.size.z);

	int i = min(nx, width-1);
	int j = min(ny, height-1);
	int k = min(nz, depth-1);

	//cout << "point: " << p << endl;
	//cout << "cell: " << i << ", " << j << ", " << k << endl;
	//float gap = minGapAtTargetArea(p, i, j, k, 0);
	//if(gap > minGap){
	GridIndex index(i,j,k);
	if(find(index) == end()){
		this->operator[](index) = new GridCell(this, index);
	}
	//this->operator[](index)->add(p);

	float gap = minGapAtTargetArea(p, i, j, k);
	oMinGap = gap;
	if(gap >= minGap){
	//if(isDistant(p, i, j, k)){
		this->operator[](index)->add(p);
		return true;
	}else{
		return false;
	}
}

bool SparseGrid::add(Point &p){
	//cout << "SparseGrid::add(" << p << ")" << endl;
	int nx = (int)(width*(p.x - aabb.min.x) / aabb.size.x);
	int ny = (int)(height*(p.y - aabb.min.y) / aabb.size.y);
	int nz = (int)(depth*(p.z - aabb.min.z) / aabb.size.z);

	int i = min(nx, width-1);
	int j = min(ny, height-1);
	int k = min(nz, depth-1);

	//cout << "point: " << p << endl;
	//cout << "cell: " << i << ", " << j << ", " << k << endl;
	//float gap = minGapAtTargetArea(p, i, j, k, 0);
	//if(gap > minGap){
	GridIndex index(i,j,k);
	if(find(index) == end()){
		this->operator[](index) = new GridCell(this, index);
	}
	//this->operator[](index)->add(p);

	if(isDistant(p, i, j, k)){
		this->operator[](index)->add(p);
		return true;
	}else{
		return false;
	}
}

void SparseGrid::addWithoutCheck(Point &p){
	int nx = (int)(width*(p.x - aabb.min.x) / aabb.size.x);
	int ny = (int)(height*(p.y - aabb.min.y) / aabb.size.y);
	int nz = (int)(depth*(p.z - aabb.min.z) / aabb.size.z);

	int i = min(nx, width-1);
	int j = min(ny, height-1);
	int k = min(nz, depth-1);

	GridIndex index(i,j,k);
	if(find(index) == end()){
		this->operator[](index) = new GridCell(this, index);
	}

	this->operator[](index)->add(p);
}