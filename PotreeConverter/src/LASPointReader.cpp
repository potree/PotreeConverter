

#include "LASPointReader.h"

#include <fstream>
#include <iostream>


using std::ifstream;
using std::cout;
using std::endl;

LASPointReader::LASPointReader(string file)
	: stream(file, std::ios::in | std::ios::binary)
	, reader(liblas::ReaderFactory().CreateWithStream(stream))
{
	liblas::Header const& header = reader.GetHeader();
	liblas::Bounds<double> const &extent = header.GetExtent();
	Vector3 min = Vector3(extent.minx(), extent.miny(), extent.minz());
	Vector3 max = Vector3(extent.maxx(), extent.maxy(), extent.maxz());
	aabb = AABB(min, max);
}

void LASPointReader::close(){
	stream.close();
}

long LASPointReader::numPoints(){
	return reader.GetHeader().GetPointRecordsCount();
}

bool LASPointReader::readNextPoint(){
	return reader.ReadNextPoint();
}

Point LASPointReader::getPoint(){
	liblas::Point const &point = reader.GetPoint();
	float x = point.GetX();
	float y = point.GetY();
	float z = point.GetZ();
	char r = (unsigned char)(float(point.GetColor().GetRed()) / 256.0f);
	char g = (unsigned char)(float(point.GetColor().GetGreen()) / 256.0f);
	char b = (unsigned char)(float(point.GetColor().GetBlue()) / 256.0f);
	Point p(x,y,z,r,g,b);

	return p;
}

AABB LASPointReader::getAABB(){
	return aabb;
}