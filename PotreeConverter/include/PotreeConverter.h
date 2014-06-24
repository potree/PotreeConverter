

#ifndef POTREE_CONVERTER_H
#define POTREE_CONVERTER_H

#define POTREE_FORMAT_VERSION "1.1"

#include "AABB.h"
#include "PointReader.h"

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cstdint>


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
	map<string, PointReader*> reader;
	map<string, PointReader*>::iterator currentReader;
	AABB aabb;
	vector<string> fData;
	string workDir;
	float minGap;
	stringstream cloudJs;
	int maxDepth;
	string format;
	float range;

	char *buffer;

	bool readNextPoint();
	Point getPoint();

public:

	PotreeConverter(vector<string> fData, string workDir, float minGap, int maxDepth, string format, float range);

	void convert(uint64_t numPoints);
	void convert();
	void initReader();
	void saveCloudJS();



	/**
	 * converts points in fData
	 *
	 * @returns a list of indices of the octree nodes that were created
	 */
	ProcessResult process(string source, string target, AABB aabb, int depth);




};


#endif