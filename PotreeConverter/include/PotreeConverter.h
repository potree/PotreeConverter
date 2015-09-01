

#ifndef POTREE_CONVERTER_H
#define POTREE_CONVERTER_H

#include "AABB.h"
#include "CloudJS.hpp"
#include "definitions.hpp"
#include "PointReader.h"

#include <string>
#include <vector>
#include <cstdint>

using std::vector;
using std::string;

namespace Potree{

class SparseGrid;

class PotreeConverter{

private:
	AABB aabb;
	vector<string> sources;
	string workDir;
	CloudJS cloudjs;
	PointAttributes pointAttributes;

	PointReader *createPointReader(string source, PointAttributes pointAttributes);
	void prepare();
	AABB calculateAABB();

public:
	float spacing;
	int maxDepth;
	string format;
	OutputFormat outputFormat;
	vector<string> outputAttributes;
	vector<double> colorRange;
	vector<double> intensityRange;
	double scale = 0.01;
	int diagonalFraction = 250;
	vector<double> aabbValues;
	string pageName = "";
	StoreOption storeOption = StoreOption::ABORT_IF_EXISTS;

	PotreeConverter(string workDir, vector<string> sources);
		
	void convert();

};

}

#endif