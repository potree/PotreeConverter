

#ifndef POTREE_CONVERTER_H
#define POTREE_CONVERTER_H

#define POTREE_FORMAT_VERSION "1.1"

#include "AABB.h"
#include "PointReader.h"

#include <string>
#include <vector>
#include <sstream>


using std::vector;
using std::string;
using std::stringstream;


class PotreeConverter{

private:
	PointReader *reader;
	AABB aabb;
	string fData;
	string workDir;
	float minGap;
	stringstream cloudJs;
	int maxDepth;
	string format;
	float range;

	char *buffer;

public:

	PotreeConverter(string fData, string workDir, float minGap, int maxDepth, string format, float range);

	void convert(int numPoints);
	void convert();
	void initReader();



	/**
	 * converts points in fData
	 *
	 * @returns a list of indices of the octree nodes that were created
	 */
	vector<int> process(string source, string target, AABB aabb, int depth);




};


#endif