

#ifndef POTREE_CONVERTER_H
#define POTREE_CONVERTER_H

#define POTREE_FORMAT_VERSION "1.0"

#include "AABB.h"

#include <string>
#include <vector>
#include <sstream>


using std::vector;
using std::string;
using std::stringstream;


class PotreeConverter{

private:
	AABB aabb;
	string fData;
	string workDir;
	float minGap;
	stringstream cloudJs;
	int maxDepth;

	char *buffer;

public:

	PotreeConverter(string fData, string workDir, float minGap, int maxDepth){
		this->fData = fData;
		this->workDir = workDir;
		this->minGap = minGap;
		this->maxDepth = maxDepth;
		buffer = new char[4*10*1000*1000*sizeof(float)];
	}

	void convert(int numPoints);
	void convert();



	/**
	 * converts points in fData
	 *
	 * @returns a list of indices of the octree nodes that were created
	 */
	vector<int> process(string source, string target, AABB aabb, int depth);




};


#endif