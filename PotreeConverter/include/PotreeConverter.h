

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
#include <boost/thread.hpp>
#include <boost/atomic.hpp>

class SparseGrid;


using std::vector;
using std::string;
using std::stringstream;
using std::map;
using boost::atomic;
using boost::mutex;

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
	vector<double> colorRange;
	vector<double> intensityRange;
	double scale;
	int diagonalFraction;

	PointReader *createPointReader(string source);
    mutex popLasMutex;
    void lasThread(list<string> &sources, list<int> &indexes, atomic<long long> &pointsProcessed);

public:

	PotreeConverter(vector<string> fData, string workDir, float spacing, int diagonalFraction, int maxDepth, string format, vector<double> colorRange, vector<double> intensityRange, double scale, OutputFormat outFormat);

	void convert();

};

#endif