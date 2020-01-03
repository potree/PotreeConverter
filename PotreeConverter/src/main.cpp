
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include <fstream>

#include <filesystem>

#include "Subsampler.h"
#include "Subsampler_PoissonDisc.h"
#include "Metadata.h"
#include "LASLoader.hpp"
#include "Chunker.h"
#include "Vector3.h"
#include "ChunkProcessor.h"

using namespace std::experimental;

namespace fs = std::experimental::filesystem;


void saveChunks(Chunker* chunker) {

	//std::ios_base::sync_with_stdio(false);

	int64_t sum = 0;

	for (int i = 0; i < chunker->cells.size(); i++) {
		Cell& cell = chunker->cells[i];

		if (cell.count == 0) {
			continue;
		}

		string chunkDirectory = chunker->targetDirectory + "/chunks/";
		string path = chunkDirectory + "chunk_" + std::to_string(i) + ".bin";

		fs::create_directories(chunkDirectory);

		auto file = std::fstream(path, std::ios::out | std::ios::binary);

		for (Points* batch : cell.batches) {
			sum += batch->points.size();
		}

		for (Points* batch : cell.batches) {

			vector<Vector3<double>> coordinates;
			for (Point& point : batch->points) {
				coordinates.emplace_back(point.x, point.y, point.z);
			}

			const char* data = reinterpret_cast<const char*>(coordinates.data());
			int64_t size = coordinates.size() * sizeof(Vector3<double>);

			//const char* data = reinterpret_cast<const char*>(batch->data[0]->dataU8);
			//int64_t size = batch->data[0]->size;

			file.write(data, size);

		}

		for (Points* batch : cell.batches) {
			const char* data = reinterpret_cast<const char*>(batch->attributeBuffer->dataU8);
			int64_t size = batch->attributeBuffer->size;

			file.write(data, size);
		}

		file.close();



	}

	cout << "sum: " << sum << endl;

}

int gridSizeFromPointCount(uint64_t pointCount) {
	if (pointCount < 10'000'000) {
		return 2;
	} if (pointCount < 100'000'000) {
		return 4;
	} else if (pointCount < 1'000'000'000) {
		return 8;
	} else if (pointCount < 10'000'000'000) {
		return 16;
	} else if (pointCount < 100'000'000'000) {
		return 32;
	} else{
		return 64;
	}
}


future<Chunker*> chunking(LASLoader* loader, Metadata metadata) {

	Chunker* chunker = new Chunker(metadata.targetDirectory, metadata.chunkGridSize);

	Vector3<double> size = metadata.max - metadata.min;
	double cubeSize = std::max(std::max(size.x, size.y), size.z);
	Vector3<double> cubeMin = metadata.min;
	Vector3<double> cubeMax = cubeMin + cubeSize;

	chunker->min = cubeMin;
	chunker->max = cubeMax;

	int batchNumber = 0;
	Points* batch = co_await loader->nextBatch();
	while (batch != nullptr) {
		cout << "batch loaded: " << batchNumber << endl;

		chunker->add(batch);

		batch = co_await loader->nextBatch();

		batchNumber++;
	}

	saveChunks(chunker);

	return chunker;
}

future<void> run() {

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/archpro/heidentor.las";
	//string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	string path = "D:/dev/pointclouds/open_topography/ca13/morro_rock/merged.las";
	string targetDirectory = "C:/temp/test";

	auto tStart = now();

	LASLoader* loader = new LASLoader(path);

	Metadata metadata;
	metadata.targetDirectory = targetDirectory;
	metadata.min = loader->min;
	metadata.max = loader->max;
	metadata.numPoints = loader->numPoints;
	//metadata.chunkGridSize = gridSizeFromPointCount(metadata.numPoints);
	metadata.chunkGridSize = 4;

	//Chunker* chunker = co_await chunking(loader, metadata);

	vector<Chunk*> chunks = getListOfChunks(metadata);

	vector<thread> threads;
	for (Chunk* chunk : chunks) {

		threads.emplace_back(thread([chunk](){
			loadChunk(chunk);
			processChunk(chunk);
		}));

	}

	for (thread& t : threads) {
		t.join();
	}

	


	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	co_return;
}

int main(int argc, char **argv){

	//cout << sizeof(Point) << endl;

	run().wait();

	return 0;
}

