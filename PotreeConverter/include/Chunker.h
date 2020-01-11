
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>
#include <atomic>
#include <thread>


#include "Points.h"
#include "Vector3.h"
#include "LASWriter.hpp"
#include "TaskPool.h"

#include "json.hpp"

using json = nlohmann::json;

using std::string;
using std::atomic_bool;
namespace fs = std::experimental::filesystem;

struct ChunkerCell {
	uint32_t count = 0;
	vector<shared_ptr<Points>> batches;
	//bool flushing = false;
	atomic<bool> isFlusing = false;
	int index = 0;
	int ix = 0;
	int iy = 0;
	int iz = 0;
	string name = "";

	ChunkerCell() {
		//flushing = false;
	}
};

// might be better off using https://github.com/progschj/ThreadPool
struct FlushTask {
	shared_ptr<ChunkerCell> cell;
	string filepath = "";
	vector<shared_ptr<Points>> batches;
};

mutex mtx_abc;

auto flushProcessor = [](shared_ptr<FlushTask> task) {

	double tStart = now();

	uint64_t numPoints = 0;

	for (auto batch : task->batches) {
		numPoints += batch->points.size();
	}

	uint64_t bytesPerPoint = 28;
	uint64_t fileDataSize = numPoints * bytesPerPoint;
	void* fileData = malloc(fileDataSize);
	uint8_t* fileDataU8 = reinterpret_cast<uint8_t*>(fileData);

	uint64_t i = 0;
	for (shared_ptr<Points> batch : task->batches) {

		uint8_t* attBuffer = batch->attributeBuffer->dataU8;
		int attributesByteSize = 4;

		//vector<uint8_t> dbgSrc(attBuffer, attBuffer + batch->attributeBuffer->size);

		for (Point& point : batch->points) {

			int fileDataOffset = i * bytesPerPoint;

			memcpy(fileDataU8 + fileDataOffset, &point, 24);
			
			uint8_t* attSrc = attBuffer + (point.index * attributesByteSize);
			memcpy(fileDataU8 + fileDataOffset + 24, attSrc, attributesByteSize);

			i++;
		}
	}

	lock_guard<mutex> lock(mtx_abc);

	fstream file;
	file.open(task->filepath, ios::out | ios::binary | ios::app);
	file.write(reinterpret_cast<const char*>(fileData), fileDataSize);
	file.close();

	//cout << task->filepath << endl;

	free(fileData);

	task->cell->isFlusing = false;

	// shouldn't be necessary with shared pointers?
	//for (auto batch : task->batches) {
	//	delete batch;
	//}

	double duration = now() - tStart;

};

class Chunker {
public:

	vector<Points*> batchesToDo;
	int32_t gridSize = 1; 
	string path = "";
	Attributes attributes;

	vector<shared_ptr<ChunkerCell>> cells;
	Vector3<double> min;
	Vector3<double> max;
	atomic<int> flushThreads = 0;

	// debug
	mutex mtx_debug_message;
	vector<string> debugMessages;
	
	shared_ptr<TaskPool<FlushTask>> pool;

	Chunker(string path, Attributes attributes, Vector3<double> min, Vector3<double> max, int gridSize) {
		this->path = path;
		this->attributes = attributes;
		this->min = min;
		this->max = max;
		this->gridSize = gridSize;


		int numCells = gridSize * gridSize * gridSize;
		cells.resize(numCells, nullptr);

		int numFlushThreads = 10;
		pool = make_shared<TaskPool<FlushTask>>(numFlushThreads, flushProcessor);

	}

	void add(shared_ptr<Points> batch) {

		Attributes attributes = batch->attributes;

		int attributesByteSize = 4;

		double gridSizeD = double(gridSize);
		Vector3<double> size = max - min;
		Vector3<double> cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);

		int64_t numPoints = batch->points.size();
		int numCells = gridSize * gridSize * gridSize;
		vector<int> cells_numNew(numCells);

		for (int64_t i = 0; i < numPoints; i++) {
			Point& point = batch->points[i];

			double x = point.x;
			double y = point.y;
			double z = point.z;

			int32_t ux = int32_t(cellsD.x * (x - min.x) / size.x);
			int32_t uy = int32_t(cellsD.y * (y - min.y) / size.y);
			int32_t uz = int32_t(cellsD.z * (z - min.z) / size.z);

			ux = std::min(ux, gridSize - 1);
			uy = std::min(uy, gridSize - 1);
			uz = std::min(uz, gridSize - 1);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			cells_numNew[index]++;

		}

		// allocate necessary space for each cell
		for (uint64_t i = 0; i < cells_numNew.size(); i++) {
			int numNew = cells_numNew[i];

			if (numNew == 0) {
				continue;
			}

			uint64_t attributeBufferSize = numNew * attributes.byteSize;

			auto cellBatch = make_shared<Points>();
			cellBatch->attributeBuffer = new Buffer(attributeBufferSize);
			cellBatch->attributes = attributes;

			if (cells[i] == nullptr) {
				auto cell = make_shared<ChunkerCell>();
				cell->index = i; 

				int ix = i % gridSize;
				int iy = ((i - ix) / gridSize) % gridSize;
				int iz = (i - ix - iy * gridSize) / (gridSize * gridSize);

				cell->ix = ix;
				cell->iy = iy;
				cell->iz = iz;

				string name = "r";
				int levels = std::log2(gridSize);

				int div = gridSize;
				for (int j = 0; j < levels; j++) {

					int lIndex = 0;

					if (ix >= (div / 2)) {
						lIndex = lIndex + 0b100;
						ix = ix - div / 2;
					}

					if (iy >= (div / 2)) {
						lIndex = lIndex + 0b010;
						iy = iy - div / 2;
					}

					if (iz >= (div / 2)) {
						lIndex = lIndex + 0b001;
						iz = iz - div / 2;
					}

					name += to_string(lIndex);
					div = div / 2;
				}
				cell->name = name;

				cells[i] = cell;
				
			}

			cells[i]->batches.push_back(cellBatch);
		}

		vector<shared_ptr<ChunkerCell>> toFlush;

		// now add them
		for (int64_t i = 0; i < batch->points.size(); i++) {
			Point point = batch->points[i];
			double x = point.x;
			double y = point.y;
			double z = point.z;

			int32_t ux = int32_t(cellsD.x * (x - min.x) / size.x);
			int32_t uy = int32_t(cellsD.y * (y - min.y) / size.y);
			int32_t uz = int32_t(cellsD.z * (z - min.z) / size.z);

			ux = std::min(ux, gridSize - 1);
			uy = std::min(uy, gridSize - 1);
			uz = std::min(uz, gridSize - 1);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			auto cell = cells[index];

			auto cellBatch = cell->batches.back();
			uint8_t* attBuffer = batch->attributeBuffer->dataU8;

			// copy point and its attribute buffer
			uint64_t srcIndex = point.index;
			uint64_t destIndex = cellBatch->points.size();

			point.index = destIndex;

			cellBatch->points.push_back(point);

			uint8_t* attDest = cellBatch->attributeBuffer->dataU8 + (destIndex * attributesByteSize);
			uint8_t* attSrc = batch->attributeBuffer->dataU8 + (srcIndex * attributesByteSize);
			memcpy(attDest, attSrc, attributesByteSize);

			//vector<uint8_t> dbgSrc(attSrc, attSrc + 4);
			//vector<uint8_t> dbgDest(attDest, attDest + 4);
			
			cell->count++;

			if (cell->count > 1'000'000 && cell->isFlusing == false) {
				toFlush.push_back(cell);
				cell->isFlusing = true;
			}
		}

		//for (ChunkerCell* cell : cells) {
		//	if (cell == nullptr) {
		//		continue;
		//	}

		//	for (Points* batch : cell->batches) {
		//		vector<uint8_t> dbgSrc(
		//			batch->attributeBuffer->dataU8,
		//			batch->attributeBuffer->dataU8 + batch->attributeBuffer->size);

		//		int a = 10;
		//	}
		//}
		

		for (auto cell : toFlush) {
			flushCell(cell);
		}

	}

	void addDebugMessage(string message) {

		lock_guard<mutex> lock(mtx_debug_message);

		debugMessages.push_back(message);

	}

	void flushCell(shared_ptr<ChunkerCell> cell) {

		auto task = make_shared<FlushTask>();
		task->cell = cell;
		task->filepath = path + "/" + cell->name + ".bin";
		task->batches = std::move(cell->batches);

		cell->count = 0;
		cell->batches = vector<shared_ptr<Points>>();
		cell->isFlusing = true;

		pool->addTask(task);

	}

	void saveMetadata() {

		string filepath = path + "/chunks.json";

		auto min = this->min;
		auto max = this->max;

		json js = {
			{"min", {min.x, min.y, min.z}},
			{"max", {max.x, max.y, max.z}},
		};

		fstream file;
		file.open(filepath, ios::out);
		file << js.dump(4);
		file.close();

	}

	void close() {

		// finish all other flushes first
		// used to make sure that we are not flushing to a file that's already being flushed
		pool->waitTillEmpty();

		vector<shared_ptr<ChunkerCell>> populatedCells;
		int numCells = 0;
		for (auto cell : cells) {
			if (cell != nullptr && cell->count > 0) {
				populatedCells.push_back(cell);
			}
		}

		// now flush all the remaining cells
		for (auto cell : populatedCells) {
			flushCell(cell);
		}


		saveMetadata();


		cout << "waiting flush" << endl;
		pool->close();
		cout << "all flushed" << endl;


	}

};