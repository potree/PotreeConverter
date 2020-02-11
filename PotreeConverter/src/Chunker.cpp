

#include "Chunker.h"

#include <string>
#include <assert.h>
#include <filesystem>
#include <functional>
#include <fstream>
#include <unordered_map>
#include <memory>

#include "json.hpp"

#include "LASLoader.hpp"
#include "LASWriter.hpp"
#include "TaskPool.h"

mutex mtx_find_mutex;
//unordered_map<int, shared_ptr<mutex>> mutexes;
unordered_map<int, mutex> mutexes;

using std::string;
using std::to_string;
using std::ios;
using std::make_shared;
using json = nlohmann::json;

using std::fstream;
namespace fs = std::filesystem;


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

shared_ptr< TaskPool<ChunkPiece>> flushPool;

void writeMetadata(string path, Vector3<double> min, Vector3<double> max) {

	json js;

	js["min"] = { min.x, min.y, min.z };
	js["max"] = { max.x, max.y, max.z };

	string content = js.dump(4);

	writeFile(path, content);

}



Chunker::Chunker(string path, Attributes attributes, Vector3<double> min, Vector3<double> max, int gridSize) {
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

void Chunker::close() {
	flushPool->close();
}

string Chunker::getName(int index) {

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

void Chunker::add(shared_ptr<Points> batch) {

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

		flushPool->addTask(piece);
	}
}



void doChunking(string pathIn, string pathOut) {
	double tStart = now();

	flushPool = make_shared<TaskPool<ChunkPiece>>(16, flushProcessor);

	LASLoader* loader = new LASLoader(pathIn, 2);
	loader->spawnLoadThread();
	Attributes attributes = loader->getAttributes();

	string path = pathOut + "/chunks/";

	fs::create_directories(path);

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		std::filesystem::remove(entry);
	}

	Vector3<double> size = loader->max - loader->min;
	double cubeSize = std::max(std::max(size.x, size.y), size.z);
	Vector3<double> cubeMin = loader->min;
	Vector3<double> cubeMax = cubeMin + cubeSize;

	int gridSize = 2;
	Chunker* chunker = new Chunker(path, attributes, cubeMin, cubeMax, gridSize);

	double sum = 0.0;
	int batchNumber = 0;
	auto promise = loader->nextBatch();
	promise.wait();
	auto batch = promise.get();

	while (batch != nullptr) {
		if ((batchNumber % 10) == 0) {
			cout << "batch loaded: " << batchNumber << endl;
		}

		auto tStart = now();
		chunker->add(batch);
		auto duration = now() - tStart;
		sum += duration;

		promise = loader->nextBatch();
		promise.wait();
		batch = promise.get();

		batchNumber++;
	}

	cout << "raw batch add time: " << sum << endl;


	string metadataPath = pathOut + "/chunks/metadata.json";
	writeMetadata(metadataPath, cubeMin, cubeMax);




	printElapsedTime("chunking duration", tStart);

	flushPool->waitTillEmpty();
	//chunker->close();

	printElapsedTime("chunking duration + close", tStart);
}

