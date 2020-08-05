
#include "LasLoader.h"
#include "unsuck/unsuck.hpp"

#include <memory>

using std::ios;
using std::make_shared;

//template<typename T>
//T read(vector<uint8_t>& buffer, int offset) {
//	T value = reinterpret_cast<T*>(buffer.data() + offset)[0];
//
//	return value;
//}

int getRgbOffset(int format) {
	if (format == 2) {
		return 20;
	} else if (format == 3) {
		return 28;
	} else if (format == 5) {
		return 28;
	} else {
		return 0;
	}
}

vector<VLR> loadVLRs(string path, LasHeader& header) {
	auto offset = header.headerSize;
	auto numVLRs = header.numVLRs;
	constexpr size_t vlrHeaderSize = 54;

	vector<VLR> vlrs;
	vlrs.reserve(numVLRs);

	for (uint32_t i = 0; i < numVLRs; i++) {
		auto headerData = readBinaryFile(path, offset, vlrHeaderSize);
		offset += vlrHeaderSize;

		VLR vlr;
		memcpy(vlr.userID, headerData.data() + 2, 16);
		vlr.recordID = read<uint16_t>(headerData, 18);
		vlr.recordLengthAfterHeader = read <uint16_t>(headerData, 20);
		memcpy(vlr.description, headerData.data() + 22, 32);

		vlr.data = readBinaryFile(path, offset, vlr.recordLengthAfterHeader);

		//cout << "== VLR ==" << endl;
		//cout << "id: " << vlr.recordID << endl;
		//cout << "size: " << vlr.recordLengthAfterHeader << endl;
		//cout << "==========" << endl;

		vlrs.push_back(vlr);
	}

	return vlrs;
}

vector<ExtendedVLR> loadExtendedVLRs(string path, LasHeader& header) {
	auto offset = header.offsetToExtendedVLRs;
	auto numExtendedVLRs = header.numExtendedVLRs;
	constexpr size_t evlrHeaderSize = 60;

	vector<ExtendedVLR> evlrs;
	evlrs.reserve(numExtendedVLRs);

	for (uint32_t i = 0; i < numExtendedVLRs; i++) {
		auto headerData = readBinaryFile(path, offset, evlrHeaderSize);
		offset += evlrHeaderSize;

		ExtendedVLR evlr;
		memcpy(evlr.userID, headerData.data() + 2, 16);
		evlr.recordID = read<uint16_t>(headerData, 18);
		evlr.recordLengthAfterHeader = read <uint64_t>(headerData, 20);
		memcpy(evlr.description, headerData.data() + 28, 32);

		evlr.data = readBinaryFile(path, offset, evlr.recordLengthAfterHeader);

		//cout << "== eVLR ==" << endl;
		//cout << "id: " << evlr.recordID << endl;
		//cout << "size: " << evlr.recordLengthAfterHeader << endl;
		//cout << "==========" << endl;

		evlrs.push_back(evlr);
	}

	return evlrs;
}

LasHeader loadHeader(string path) {

	LasHeader header;

	header.headerSize = 227;
	auto buffer = readBinaryFile(path, 0, header.headerSize);

	header.versionMajor = read<uint8_t>(buffer, 24);
	header.versionMinor = read<uint8_t>(buffer, 25);
	header.headerSize = read<uint16_t>(buffer, 94);

	buffer = readBinaryFile(path, 0, header.headerSize);

	header.offsetToPointData = read<uint32_t>(buffer, 96);
	header.numVLRs = read<uint32_t>(buffer, 100);
	header.pointDataFormat = read<uint8_t>(buffer, 104);
	header.pointDataRecordLength = read<uint16_t>(buffer, 105);
	header.numPoints = read<uint32_t>(buffer, 107);

	header.scale[0] = read<double>(buffer, 131);
	header.scale[1] = read<double>(buffer, 139);
	header.scale[2] = read<double>(buffer, 147);

	header.offset[0] = read<double>(buffer, 155);
	header.offset[1] = read<double>(buffer, 163);
	header.offset[2] = read<double>(buffer, 171);

	header.min[0] = read<double>(buffer, 187);
	header.min[1] = read<double>(buffer, 203);
	header.min[2] = read<double>(buffer, 219);

	header.max[0] = read<double>(buffer, 179);
	header.max[1] = read<double>(buffer, 195);
	header.max[2] = read<double>(buffer, 211);

	bool isExtentedHeader = header.versionMajor >= 1 && header.versionMinor >= 4;
	if (isExtentedHeader) {
		header.numPoints = read<uint64_t>(buffer, 247);
		header.offsetToExtendedVLRs = read<uint64_t>(buffer, 235);
		header.numExtendedVLRs = read<uint32_t>(buffer, 243);
	}

	header.extendedVLRs = loadExtendedVLRs(path, header);
	header.vlrs = loadVLRs(path, header);

	if (iEndsWith(path, "laz")) {
		header.pointDataFormat -= 128;
	}

	//cout << "header.headerSize: " << header.headerSize << endl;
	//cout << "header.offsetToPointData: " << header.offsetToPointData << endl;
	//cout << "header.pointDataFormat: " << header.pointDataFormat << endl;
	//cout << "header.pointDataRecordLength: " << header.pointDataRecordLength << endl;
	//cout << "header.numPoints: " << header.numPoints << endl;

	return header;
}



LasLoader::LasLoader(string path, int numThreads) {
	this->path = path;
	this->header = loadHeader(path);

	auto processor = [this](shared_ptr<LasLoadTask> task) {

		uint64_t firstPoint = task->firstPoint;
		uint64_t numPoints = task->numPoints;
		auto batch = this->readBatch(firstPoint, numPoints);

		lock_guard<mutex> lock(mtx_loadedBatches);
		loadedBatches.push_back(batch);
	};

	pool = make_shared<TaskPool<LasLoadTask>>(numThreads, processor);

	for (uint64_t i = 0; i < header.numPoints; i += batchSize) {
		uint64_t firstPoint = i;
		int taskSize = batchSize;

		if (firstPoint + taskSize > header.numPoints) {
			taskSize = header.numPoints - firstPoint;
		}

		auto task = make_shared<LasLoadTask>(firstPoint, taskSize);
		pool->addTask(task);
	}
}

shared_ptr<Batch> LasLoader::nextBatch() {

	while (true) {

		{
			lock_guard<mutex> lock(mtx_loadedBatches);

			if (loadedBatches.size() == 0 && pool->isWorkDone()) {
				break;
			} else if (loadedBatches.size() > 0) {
				auto batch = loadedBatches.back();
				loadedBatches.pop_back();

				return batch;
			}
		}
		

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return nullptr;
}

shared_ptr<Batch> LasLoader::readBatch(uint64_t firstPoint, uint64_t numPoints) {

	auto batch = make_shared<Batch>();
	batch->points.reserve(numPoints);
	//batch->points.resize(numPoints);
	//batch->points1 = reinterpret_cast<Point*>(malloc(numPoints * sizeof(Point)));

	int bytesPerPoint = header.pointDataRecordLength;
	int bufferSize = bytesPerPoint * numPoints;
	
	uint64_t start = header.offsetToPointData + firstPoint * bytesPerPoint;
	uint64_t size = numPoints * bytesPerPoint;
	auto data = readBinaryFile(path, start, size);
	auto buffer = data.data();

	int rgbOffset = getRgbOffset(header.pointDataFormat);

	 auto scaleX = header.scale[0];
	 auto scaleY = header.scale[1];
	 auto scaleZ = header.scale[2];

	 auto offsetX = header.offset[0];
	 auto offsetY = header.offset[1];
	 auto offsetZ = header.offset[2];

	 int pointOffset = 0;
	 for (int i = 0; i < numPoints; i++) {
	 	int32_t* xyz = reinterpret_cast<int32_t*>(buffer + pointOffset);

	 	double x = double(xyz[0]) * scaleX + offsetX;
	 	double y = double(xyz[1]) * scaleY + offsetY;
	 	double z = double(xyz[2]) * scaleZ + offsetZ;

	 	uint16_t* rgb = reinterpret_cast<uint16_t*>(buffer + pointOffset + rgbOffset);
		
	 	uint8_t r = rgb[0] < 256 ? rgb[0] : rgb[0] / 256;
	 	uint8_t g = rgb[1] < 256 ? rgb[1] : rgb[1] / 256;
	 	uint8_t b = rgb[2] < 256 ? rgb[2] : rgb[2] / 256;


		// use vectors, ~0.48 in some benchmarks
	 	Point point = {x, y, z, r, g, b, 255};
	 	//batch->points.push_back(point);
		batch->points.emplace_back(x, y, z, r, g, b, 255);





		// use pointer/array, ~0.42s in some benchmarks
		//Point& point = batch->points1[i];

		//point.x = x;
		//point.y = y;
		//point.z = z;

		//point.r = r;
		//point.g = g;
		//point.b = b;
		//point.a = 255;

	 	pointOffset += bytesPerPoint;
	 }

	return batch;

	//return nullptr;
}