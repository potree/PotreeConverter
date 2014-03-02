

#ifndef BINPOINTREADER_H
#define BINPOINTREADER_H

#include "Point.h"
#include "PointReader.h"

#include <string>
#include <iostream>
#include <fstream>

using std::string;
using std::ifstream;
using std::cout;
using std::endl;


class BinPointReader : public PointReader{
private:
	AABB aabb;
	char *buffer;
	ifstream stream;
	int batchSize;
	int batchByteSize;
	float *points;
	char *cPoints;
	string file;
	long offset;
	long localOffset;
	long currentBatchPointCount;

public:

	BinPointReader(string file);

	bool readNextPoint();

	Point getPoint();

	AABB getAABB();

	long numPoints();

	void close();
};

#endif