

#ifndef POTREE_CONVERTER_H
#define POTREE_CONVERTER_H

#define POTREE_FORMAT_VERSION "1.2"

#include "AABB.h"
#include "CloudJS.hpp"
#include "definitions.hpp"
#include "PointReader.h"

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cstdint>

class SparseGrid;


using std::vector;
using std::string;
using std::stringstream;
using std::map;

struct ProcessResult{
	vector<int> indices;
	uint64_t numAccepted;
	uint64_t numRejected;

	ProcessResult(vector<int> indices, uint64_t numAccepted, uint64_t numRejected){
		this->indices = indices;
		this->numAccepted = numAccepted;
		this->numRejected = numRejected;
	}
};

class PotreeConverter{

private:
	AABB aabb;
	vector<string> sources;
	string workDir;
	float spacing;
	CloudJS cloudjs;
	int maxDepth;
	string format;
	OutputFormat outputFormat;
	vector<string> outputAttributes;
	vector<double> colorRange;
	vector<double> intensityRange;
	double scale;
	int diagonalFraction;

	PointReader *createPointReader(string source, PointAttributes pointAttributes);

public:

	PotreeConverter(
		vector<string> fData, 
		string workDir, 
		float spacing, 
		int diagonalFraction, 
		int maxDepth, 
		string format, 
		vector<double> colorRange, 
		vector<double> intensityRange, 
		double scale, 
		OutputFormat outFormat,
		vector<string> outputAttributes);

	void convert();

};

#endif