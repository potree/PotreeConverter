

#ifndef LASPOINTREADER_H
#define LASPOINTREADER_H

#include <string>
#include <iostream>
#include <vector>

#include "lasreader.hpp"
#include "lasdefinitions.hpp"

#include "Point.h"
#include "PointReader.h"

using std::string;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;




class LASPointReader : public PointReader{
private:
	AABB aabb;
	string path;
	LASreader *reader;
	vector<string> files;
	vector<string>::iterator currentFile;

public:

	LASPointReader(string path);

	~LASPointReader();

	bool readNextPoint();

	Point getPoint();

	AABB getAABB();

	long numPoints();

	void close();

	Vector3<double> getScale();

	LASheader const &getHeader();
};

#endif