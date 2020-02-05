
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>
#include <functional>

#include "Points.h"
#include "Vector3.h"
#include "LASWriter.hpp"
#include "TaskPool.h"

using std::string;

struct ChunkPiece {
	int index = -1;
	string name = "";
	string path = "";
	shared_ptr<Points> points;

	ChunkPiece(int index, string name, string path, shared_ptr<Points> points) {
		this->index = index;
		this->name = name;
		this->path = path;
		this->points = points;
	}
};

mutex mtx_find_mutex;
//unordered_map<int, shared_ptr<mutex>> mutexes;
unordered_map<int, mutex> mutexes;

void flushProcessor(shared_ptr<ChunkPiece> piece) {

	auto points = piece->points;
	uint64_t numPoints = points->points.size();
	uint64_t bytesPerPoint = 28;
	uint64_t fileDataSize = numPoints * bytesPerPoint;

	void* fileData = malloc(fileDataSize);
	uint8_t* fileDataU8 = reinterpret_cast<uint8_t*>(fileData);


	uint8_t* attBuffer = points->attributeBuffer->dataU8;
	int attributesByteSize = 4;

	int i = 0;
	for (Point point : points->points) {

		int fileDataOffset = i * bytesPerPoint;

		memcpy(fileDataU8 + fileDataOffset, &point, 24);

		uint8_t* attSrc = attBuffer + (i * attributesByteSize);
		memcpy(fileDataU8 + fileDataOffset + 24, attSrc, attributesByteSize);

		i++;
	}
	
	string filepath = piece->path;

	// avoid writing to the same file by multiple threads, using one mutex per file
	mtx_find_mutex.lock();
	mutex& mtx_file = mutexes[piece->index];
	mtx_find_mutex.unlock();
	

	{
		double tLockStart = now();
		lock_guard<mutex> lock(mtx_file);
		double dLocked = now() - tLockStart;

		if (dLocked > 0.2) {
			string strDuration = to_string(dLocked);
			string msg = "long lock duration ( " + strDuration 
				+ "s) while waiting to write to " + piece->name + "\n";

			cout << msg;
		}


		fstream file;
		file.open(filepath, ios::out | ios::binary | ios::app);
		file.write(reinterpret_cast<const char*>(fileData), fileDataSize);
		file.close();
	}

	

	free(fileData);

}

TaskPool<ChunkPiece> flushPool(16, flushProcessor);


class Chunker {
public:

	string path = "";
	Attributes attributes;

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	Vector3<double> cellsD;

	int gridSize = 0;


	Chunker(string path, Attributes attributes, Vector3<double> min, Vector3<double> max, int gridSize) {
		this->path = path;
		this->min = min;
		this->max = max;
		this->attributes = attributes;
		this->gridSize = gridSize;

		double gridSizeD = double(gridSize);
		cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);
		size = max - min;

		//grid.resize(gridSize * gridSize * gridSize);
	}

	void close() {

		flushPool.close();

		//int numCells = 0;
		//int numPieces = 0;

		//for (auto cell : grid) {
		//	
		//	if (cell == nullptr) {
		//		continue;
		//	}

		//	numCells++;
		//	numPieces += cell->pieces.size();

		//	//for(auto piece : cell->pieces){

		//	//	static int i = 0;

		//	//	string lasPath = path + "/" + to_string(i) + ".las";
		//	//	LASHeader header;
		//	//	header.min = min;
		//	//	header.max = max;
		//	//	header.numPoints = piece->points.size();
		//	//	header.scale = { 0.001, 0.001, 0.001 };

		//	//	writeLAS(lasPath, header, piece->points);
		//	//	i++;
		//	//}

		//}

		//cout << "#cells: " << numCells << endl;
		//cout << "#pieces: " << numPieces << endl;

	}

	string getName(int index) {

		int ix = index % gridSize;
		int iy = ((index - ix) / gridSize) % gridSize;
		int iz = (index - ix - iy * gridSize) / (gridSize * gridSize);

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

		return name;
	}

	void add(shared_ptr<Points> batch) {

		int64_t gridSizeM1 = gridSize - 1;

		double scaleX = cellsD.x / size.x;
		double scaleY = cellsD.y / size.y;
		double scaleZ = cellsD.z / size.z;

		int64_t gridSize = this->gridSize;
		int64_t gridSizeGridSize = gridSize * gridSize;

		// making toIndex a lambda with necessary captures here seems to be faster than 
		// making it a member function of this class?!
		auto toIndex = [=](Point point) {
			int64_t ix = (point.x - min.x) * scaleX;
			int64_t iy = (point.y - min.y) * scaleY;
			int64_t iz = (point.z - min.z) * scaleZ;

			ix = std::min(ix, gridSizeM1);
			iy = std::min(iy, gridSizeM1);
			iz = std::min(iz, gridSizeM1);

			int64_t index = ix + gridSize * iy + gridSizeGridSize * iz;

			return index;
		};

		// compute number of points per bin
		vector<int> binCounts(gridSize * gridSize * gridSize, 0);
		for (Point point : batch->points) {

			int64_t index = toIndex(point);

			binCounts[index]++;
		}

		// create new bin-pieces and add them to bin-grid
		vector<shared_ptr<Points>> bins(gridSize * gridSize * gridSize, nullptr);
		for (int i = 0; i < binCounts.size(); i++) {

			int binCount = binCounts[i];

			if (binCount == 0) {
				continue;
			}

			shared_ptr<Points> bin = make_shared<Points>();
			bin->points.reserve(binCount);

			int attributeBufferSize = attributes.byteSize * binCount;
			bin->attributeBuffer = make_shared<Buffer>(attributeBufferSize);

			//if (grid[i] == nullptr) {
			//	grid[i] = make_shared<ChunkCell>();
			//}

			// add bin-piece to grid
			//grid[i]->pieces.push_back(bin);

			bins[i] = bin;
		}

		// fill bins
		for (Point point : batch->points) {

			int64_t index = toIndex(point);

			bins[index]->points.push_back(point);
		}

		// create flush tasks
		for (int i = 0; i < binCounts.size(); i++) {
			auto points = bins[i];

			if (points == nullptr) {
				continue;
			}

			int index = i;
			string name = getName(index);
			string filepath = this->path + name + ".bin";

			auto piece = make_shared<ChunkPiece>(index, name, filepath, points);

			flushPool.addTask(piece);
		}


	}

};