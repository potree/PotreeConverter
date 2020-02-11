
#include "LASWriter.hpp"

#include <fstream>
#include <cstdint>

using namespace std;

vector<uint8_t> makeHeaderBuffer(LASHeader header) {
	int headerSize = header.headerSize;
	vector<uint8_t> buffer(headerSize, 0);
	uint8_t* data = buffer.data();

	// file signature
	data[0] = 'L';
	data[1] = 'A';
	data[2] = 'S';
	data[3] = 'F';

	// version major & minor -> 1.4
	data[24] = 1;
	data[25] = 4;

	// header size
	reinterpret_cast<uint16_t*>(data + 94)[0] = headerSize;

	// point data format
	data[104] = 2;

	// bytes per point
	reinterpret_cast<uint16_t*>(data + 105)[0] = 26;

	// #points
	uint64_t numPoints = header.numPoints;
	reinterpret_cast<uint64_t*>(data + 247)[0] = numPoints;

	// min
	reinterpret_cast<double*>(data + 187)[0] = header.min.x;
	reinterpret_cast<double*>(data + 203)[0] = header.min.y;
	reinterpret_cast<double*>(data + 219)[0] = header.min.z;

	// offset
	reinterpret_cast<double*>(data + 155)[0] = header.min.x;
	reinterpret_cast<double*>(data + 163)[0] = header.min.y;
	reinterpret_cast<double*>(data + 171)[0] = header.min.z;

	// max
	reinterpret_cast<double*>(data + 179)[0] = header.max.x;
	reinterpret_cast<double*>(data + 195)[0] = header.max.y;
	reinterpret_cast<double*>(data + 211)[0] = header.max.z;

	// scale
	reinterpret_cast<double*>(data + 131)[0] = header.scale.x;
	reinterpret_cast<double*>(data + 139)[0] = header.scale.y;
	reinterpret_cast<double*>(data + 147)[0] = header.scale.z;

	// offset to point data
	uint32_t offSetToPointData = headerSize;
	reinterpret_cast<uint32_t*>(data + 96)[0] = offSetToPointData;

	return buffer;
}

struct LASPointF2 {
	int32_t x;
	int32_t y;
	int32_t z;
	uint16_t intensity;
	uint8_t returnNumber;
	uint8_t classification;
	uint8_t scanAngleRank;
	uint8_t userData;
	uint16_t pointSourceID;
	uint16_t r;
	uint16_t g;
	uint16_t b;
};

void writeLAS(string path, LASHeader header, vector<Point> points) {

	vector<uint8_t> headerBuffer = makeHeaderBuffer(header);

	fstream file(path, ios::out | ios::binary);

	file.write(reinterpret_cast<const char*>(headerBuffer.data()), header.headerSize);

	LASPointF2 laspoint;

	for (Point& point : points) {

		int32_t ix = int32_t((point.x - header.min.x) / header.scale.x);
		int32_t iy = int32_t((point.y - header.min.y) / header.scale.y);
		int32_t iz = int32_t((point.z - header.min.z) / header.scale.z);

		laspoint.x = ix;
		laspoint.y = iy;
		laspoint.z = iz;
		
		laspoint.r = point.index & 0xff000000;
		laspoint.g = point.index & 0x00ff0000;
		laspoint.b = point.index & 0x0000ff00;

		file.write(reinterpret_cast<const char*>(&laspoint), 26);
	}

	file.close();


}

void writeLAS(string path, LASHeader header, vector<Point> sample, Points* points) {
	vector<uint8_t> headerBuffer = makeHeaderBuffer(header);

	fstream file(path, ios::out | ios::binary);

	file.write(reinterpret_cast<const char*>(headerBuffer.data()), header.headerSize);

	LASPointF2 laspoint;
	auto attributeBuffer = points->attributeBuffer;
	int bytesPerPointAttribute = 4;

	for (Point& point : sample) {

		int32_t ix = int32_t((point.x - header.min.x) / header.scale.x);
		int32_t iy = int32_t((point.y - header.min.y) / header.scale.y);
		int32_t iz = int32_t((point.z - header.min.z) / header.scale.z);

		laspoint.x = ix;
		laspoint.y = iy;
		laspoint.z = iz;

		uint8_t* pointAttributeData = attributeBuffer->dataU8 + (point.index * bytesPerPointAttribute);
		laspoint.r = pointAttributeData[0];
		laspoint.g = pointAttributeData[1];
		laspoint.b = pointAttributeData[2];

		file.write(reinterpret_cast<const char*>(&laspoint), 26);
	}

	file.close();
}