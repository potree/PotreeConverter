
#ifndef POINTWRITER_H
#define POINTWRITER_H

#include <string>
#include <iostream>

using std::string;

class PointWriter{

public:
	string file;
	int numPoints;

	virtual void write(const Point &point) = 0;

	virtual void close() = 0;

};

#endif


