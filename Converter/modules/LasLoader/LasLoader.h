
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <thread>
#include <mutex>

using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::fstream;
using std::thread;
using std::mutex;
using std::lock_guard;

#include "unsuck/TaskPool.hpp"
#include "unsuck/unsuck.hpp"

// pack compactly to 28 bytes instead of 32 bytes
#pragma pack(push, 1)
struct Point {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;

	Point() {
		
	}

	Point(double x, double y, double z, uint8_t r, uint8_t g, uint8_t b, uint8_t a) 
	:x(x), y(y), z(z), r(r), g(g), b(b), a(a){

	}

	bool isEmpty() {
		bool value = true;
		value = value && (x == 0.0);
		value = value && (y == 0.0);
		value = value && (z == 0.0);
		return value;
	}
};
#pragma pack(pop)

struct Batch {
	vector<Point> points;
	Point* points1 = nullptr;
};

struct VLR {
	char userID[16];
	uint16_t recordID = 0;
	uint16_t recordLengthAfterHeader = 0;
	char description[32];

	vector<uint8_t> data;
};

struct ExtendedVLR {
	char userID[16];
	uint16_t recordID = 0;
	uint64_t recordLengthAfterHeader = 0;
	char description[32];

	vector<uint8_t> data;
};

// normal VLRs right after header
// extended VLRs at position specified by offsetToExtendedVLRs
struct LasHeader {

	uint16_t fileSourceID = 0;
	uint16_t globalEncoding = 0;
	uint32_t project_ID_GUID_data_1 = 0;
	uint16_t project_ID_GUID_data_2 = 0;
	uint16_t project_ID_GUID_data_3 = 0;
	uint16_t project_ID_GUID_data_4 = 0;

	uint8_t versionMajor = 1;
	uint8_t versionMinor = 4;
	string systemIdentifier = "";
	string generatingSoftware = "";
	uint16_t fileCreationDay = 0;
	uint16_t fileCreationYear = 0;

	uint16_t headerSize = 375;
	uint32_t offsetToPointData = 0;
	uint32_t numVLRs = 0;

	uint64_t offsetToExtendedVLRs = 0;
	uint32_t numExtendedVLRs = 0;

	uint8_t pointDataFormat = 0;
	uint16_t pointDataRecordLength = 0;
	uint64_t numPoints = 0;
	vector<uint32_t> numPointsPerReturn;

	double scale[3] = {0.001, 0.001, 0.001};
	double offset[3] = {0.0, 0.0, 0.0};
	double min[3] = {0.0, 0.0, 0.0};
	double max[3] = {0.0, 0.0, 0.0};

	vector<VLR> vlrs;
	vector<ExtendedVLR> extendedVLRs;

};

struct LasLoadTask {
	uint64_t firstPoint;
	uint64_t numPoints;

	LasLoadTask(uint64_t firstPoint, uint64_t numPoints) {
		this->firstPoint = firstPoint;
		this->numPoints = numPoints;
	}
};

struct LasLoader {

	string path = "";
	//fstream stream;
	LasHeader header;
	int batchSize = 1'000'000;
	bool done = false;

	mutex mtx_loadedBatches;

	vector<shared_ptr<Batch>> loadedBatches;

	shared_ptr<TaskPool<LasLoadTask>> pool;

	LasLoader(string path, int numThreads);

	shared_ptr<Batch> nextBatch();

	shared_ptr<Batch> readBatch(uint64_t firstPoint, uint64_t numPoints);


};

LasHeader loadHeader(string path);

inline vector<uint8_t> makeHeaderBuffer(LasHeader header) {
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
	reinterpret_cast<double*>(data + 187)[0] = header.min[0];
	reinterpret_cast<double*>(data + 203)[0] = header.min[1];
	reinterpret_cast<double*>(data + 219)[0] = header.min[2];

	// offset
	reinterpret_cast<double*>(data + 155)[0] = header.min[0];
	reinterpret_cast<double*>(data + 163)[0] = header.min[1];
	reinterpret_cast<double*>(data + 171)[0] = header.min[2];

	// max
	reinterpret_cast<double*>(data + 179)[0] = header.max[0];
	reinterpret_cast<double*>(data + 195)[0] = header.max[1];
	reinterpret_cast<double*>(data + 211)[0] = header.max[2];

	// scale
	reinterpret_cast<double*>(data + 131)[0] = header.scale[0];
	reinterpret_cast<double*>(data + 139)[0] = header.scale[1];
	reinterpret_cast<double*>(data + 147)[0] = header.scale[2];

	// offset to point data
	uint32_t offSetToPointData = headerSize;
	reinterpret_cast<uint32_t*>(data + 96)[0] = offSetToPointData;

	return buffer;
}