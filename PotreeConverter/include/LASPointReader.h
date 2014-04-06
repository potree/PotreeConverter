

#ifndef LASPOINTREADER_H
#define LASPOINTREADER_H

#include "Point.h"
#include "PointReader.h"

#include <liblas/liblas.hpp>

#include <string>

using std::string;


#include <iostream>


using std::ifstream;
using std::cout;
using std::endl;


class LASPointReader : public PointReader{
private:
	AABB aabb;
	ifstream stream;
	liblas::Reader reader;

public:

	LASPointReader(string file);

	bool readNextPoint();

	Point getPoint();

	AABB getAABB();

	long numPoints();

	void close();
};

#endif