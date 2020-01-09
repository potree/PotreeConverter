
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
	vector<Points*> batches;
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
	ChunkerCell* cell = nullptr;
	string filepath = "";
	vector<Points*> batches;
};

mutex mtx_abc;

auto flushProcessor = [](shared_ptr<FlushTask> task) {

	double tStart = now();

	int numPoints = 0;

	for (Points* batch : task->batches) {
		numPoints += batch->points.size();
	}

	int bytesPerPoint = 28;
	int fileDataSize = numPoints * bytesPerPoint;
	void* fileData = malloc(fileDataSize);
	uint8_t* fileDataU8 = reinterpret_cast<uint8_t*>(fileData);

	int i = 0;
	for (Points* batch : task->batches) {

		for (Point& point : batch->points) {

			int fileDataOffset = i * bytesPerPoint;

			memcpy(fileDataU8 + fileDataOffset, &point, 24);

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

	for (Points* batch : task->batches) {
		delete batch;
	}

	double duration = now() - tStart;

};

class Chunker {
public:



	vector<Points*> batchesToDo;
	int32_t gridSize = 1; 
	string path = "";

	vector<ChunkerCell*> cells;
	Vector3<double> min;
	Vector3<double> max;
	atomic<int> flushThreads = 0;

	// debug
	mutex mtx_debug_message;
	vector<string> debugMessages;
	
	shared_ptr<TaskPool<FlushTask>> pool;

	Chunker(string path, Vector3<double> min, Vector3<double> max, int gridSize) {
		this->path = path;
		this->gridSize = gridSize;
		this->min = min;
		this->max = max;

		int numCells = gridSize * gridSize * gridSize;
		cells.resize(numCells, nullptr);

		int numFlushThreads = 10;
		pool = make_shared<TaskPool<FlushTask>>(numFlushThreads, flushProcessor);

	}

	void add(Points* batch) {

		double gridSizeD = double(gridSize);
		Vector3<double> size = max - min;
		Vector3<double> cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);

		int64_t numPoints = batch->points.size();
		int numCells = gridSize * gridSize * gridSize;
		vector<int> cells_numNew(numCells);

		for (int64_t i = 0; i < numPoints; i++) {
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

			cells_numNew[index]++;

		}

		// allocate necessary space for each cell
		for (uint64_t i = 0; i < cells_numNew.size(); i++) {
			int numNew = cells_numNew[i];

			if (numNew == 0) {
				continue;
			}

			Attribute aColor;
			aColor.byteOffset = 0;
			aColor.bytes = 4;
			aColor.name = "color";

			Points* cellBatch = new Points();
			cellBatch->attributeBuffer = new Buffer(numNew * aColor.bytes);
			cellBatch->attributes.push_back(aColor);

			if (cells[i] == nullptr) {
				ChunkerCell* cell = new ChunkerCell();
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

		vector<ChunkerCell*> toFlush;

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

			ChunkerCell* cell = cells[index];

			Points* cellBatch = cell->batches.back();

			uint8_t r = batch->attributeBuffer->dataU8[4 * i + 0];
			uint8_t g = batch->attributeBuffer->dataU8[4 * i + 1];
			uint8_t b = batch->attributeBuffer->dataU8[4 * i + 2];

			int64_t offset = cellBatch->points.size();
			cellBatch->points.push_back(point);

			uint8_t* rgbBuffer = cellBatch->attributeBuffer->dataU8 + (4 * offset + 0);

			rgbBuffer[0] = r;
			rgbBuffer[1] = g;
			rgbBuffer[2] = b;
			cell->count++;

			if (cell->count > 1'000'000 && cell->isFlusing == false) {
				toFlush.push_back(cell);
				cell->isFlusing = true;
			}
		}

		for (ChunkerCell* cell : toFlush) {
			flushCell(cell);
		}

	}

	void addDebugMessage(string message) {

		lock_guard<mutex> lock(mtx_debug_message);

		debugMessages.push_back(message);

	}

	void flushCell(ChunkerCell* cell) {


		auto task = make_shared<FlushTask>();
		task->cell = cell;
		task->filepath = path + "/" + cell->name + ".bin";
		task->batches = std::move(cell->batches);

		cell->count = 0;
		cell->batches = vector<Points*>();
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

		vector<ChunkerCell*> populatedCells;
		int numCells = 0;
		for (ChunkerCell* cell : cells) {
			if (cell != nullptr && cell->count > 0) {
				populatedCells.push_back(cell);
			}
		}

		// finish all other flushes first
		// used to make sure that we are not flushing to a file that's already being flushed
		pool->waitTillEmpty();

		// now flush all the remaining cells
		for (ChunkerCell* cell : populatedCells) {
			flushCell(cell);
		}


		saveMetadata();


		cout << "waiting flush" << endl;
		pool->close();
		cout << "all flushed" << endl;


	}

};