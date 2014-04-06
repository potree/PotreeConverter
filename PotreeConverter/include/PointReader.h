

#ifndef POINTREADER_H
#define POINTREADER_H

#include "Point.h"
#include "AABB.h"

class PointReader{
public:

	virtual bool readNextPoint() = 0;

	virtual Point getPoint() = 0;

	virtual AABB getAABB() = 0;

	virtual long numPoints() = 0;

	virtual void close() = 0;
};

#endif