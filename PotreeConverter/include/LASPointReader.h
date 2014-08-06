

#ifndef LASPOINTREADER_H
#define LASPOINTREADER_H

#include <string>
#include <iostream>

#include "lasreader.hpp"
#include "lasdefinitions.hpp"

#include "Point.h"
#include "PointReader.h"

using std::string;

using std::ifstream;
using std::cout;
using std::endl;




class LASPointReader : public PointReader{
private:
	AABB aabb;
	string file;
	LASreadOpener *readOpener;
	LASreader *reader;

public:

	LASPointReader(string file);

	bool readNextPoint();

	Point getPoint();

	AABB getAABB();

	long numPoints();

	void close();

	Vector3<double> getScale();

	LASheader const &getHeader();
};

#endif