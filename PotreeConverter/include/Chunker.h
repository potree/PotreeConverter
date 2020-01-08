
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>
#include <atomic>
#include <thread>


#include "Points.h"
#include "Vector3.h"
#include "LASWriter.hpp"

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

	Chunker(string path, Vector3<double> min, Vector3<double> max, int gridSize) {
		this->path = path;
		this->gridSize = gridSize;
		this->min = min;
		this->max = max;

		int numCells = gridSize * gridSize * gridSize;
		cells.resize(numCells, nullptr);
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

				if (cell->ix > 0) {
					int a = 10;
				}

				int div = gridSize;
				for (int j = 0; j < levels; j++) {
					//double nx = double(ix) / double(div);
					//double ny = double(iy) / double(div);
					//double nz = double(iz) / double(div);

					int lIndex = 0;

					if (ix >= (div / 2)) {
						lIndex = lIndex + 0b100;
						ix = ix - div;
					}

					if (iy >= (div / 2)) {
						lIndex = lIndex + 0b010;
						iy = iy - div;
					}

					if (iz >= (div / 2)) {
						lIndex = lIndex + 0b001;
						iz = iz - div;
					}

					name += to_string(lIndex);
					div = div / 2;
				}
				
				cell->name = name;
				//cell->name = to_string(cell->ix) + "_" + to_string(cell->iy) + "_" + to_string(cell->iz);

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

		//if (cell->index != 133) {
		//	return;
		//}

		//cout << "flush " << cell->index << endl;

		double tStart = now();

		cell->count = 0;
		vector<Points*> batches = std::move(cell->batches);
		cell->batches = vector<Points*>();
		cell->isFlusing = true;

		//string filepath = path + "/chunk_" + to_string(cell->index) + ".bin";
		string filepath = path + "/" + cell->name + ".bin";
		flushThreads++;

		thread t([cell, filepath, batches, this, tStart](){

			int numPoints = 0;

			for (Points* batch : batches) {
				numPoints += batch->points.size();
			}

			addDebugMessage("start " + to_string(cell->index) + " - " + to_string(numPoints));

			//printElapsedTime("#check 1", tStart);
			fstream file;
			file.open(filepath, ios::out | ios::binary | ios::app);

			//printElapsedTime("#check 2", tStart);

			int bytesPerPoint = 28;
			int fileDataSize = numPoints * bytesPerPoint;
			void* fileData = malloc(fileDataSize);
			uint8_t* fileDataU8 = reinterpret_cast<uint8_t*>(fileData);

			//printElapsedTime("#check 3", tStart);

			int i = 0; 
			for (Points* batch : batches) {

				for (Point& point : batch->points) {

					int fileDataOffset = i * bytesPerPoint;
					
					//double* posData = reinterpret_cast<double*>(fileDataU8 + fileDataOffset);
					//posData[0] = point.x;
					//posData[1] = point.y;
					//posData[2] = point.z;

					memcpy(fileDataU8 + fileDataOffset, &point, 24);


					//uint8_t* rgbData = (fileDataU8 + fileDataOffset + 24);
					
					i++;
				}			
			}

			//printElapsedTime("#check 4", tStart);

			file.write(reinterpret_cast<const char*>(fileData), fileDataSize);

			//printElapsedTime("#check 5", tStart);

			file.close();

			//printElapsedTime("#check 6", tStart);

			free(fileData);
			//printElapsedTime("#check 7", tStart);

			cell->isFlusing = false;

			for (Points* batch : batches) {
				delete batch;
			}

			flushThreads--;
			double duration = now() - tStart;

			//printElapsedTime("#check 8", tStart);

			addDebugMessage("end " + to_string(cell->index) + " - " + to_string(numPoints) + " - " + to_string(duration) + " s");
		});
		t.detach();

	}

	void close() {

		int numCells = 0;
		for (ChunkerCell* cell : cells) {
			if (cell != nullptr && cell->count > 0) {
				numCells++;
				flushCell(cell);
				//cout << cell.count << endl;
			}
		}

		cout << numCells << endl;

		cout << "waiting flush" << endl;
		while (flushThreads > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			cout << "#flushThreads: " << flushThreads << endl;
		}
		cout << "all flushed" << endl;


	}

};