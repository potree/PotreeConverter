

#ifndef POTREE_CONVERTER_H
#define POTREE_CONVERTER_H

#define POTREE_FORMAT_VERSION "1.2"

#include "AABB.h"
#include "CloudJS.hpp"
#include "definitions.hpp"

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

	float range;

public:

	PotreeConverter(vector<string> fData, string workDir, float spacing, int maxDepth, string format, float range, OutputFormat outFormat);

	void convert();

};


#endif