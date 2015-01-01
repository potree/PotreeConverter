#ifndef TILING_GRID_H
#define TILING_GRID_H

#include <map>
#include "GridIndex.h"
#include "LASPointWriter.hpp"

#define MAX_FLOAT std::numeric_limits<float>::max()

class TilingGrid : public std::map<GridIndex, LASPointWriter*>{
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