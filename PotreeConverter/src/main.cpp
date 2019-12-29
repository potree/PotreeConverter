
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

		fs::create_directory(chunkDirectory);

		auto file = std::fstream(path, std::ios::out | std::ios::binary);

		//file.write((char*)& data[0], bytes);

		for (Points* batch : cell.batches) {
			sum += batch->count;
		}

		for (Points* batch : cell.batches) {
			const char* data = reinterpret_cast<const char*>(batch->data[0]->dataU8);
			int64_t size = batch->data[0]->size;

			file.write(data, size);

		}

		for (Points* batch : cell.batches) {
			const char* data = reinterpret_cast<const char*>(batch->data[1]->dataU8);
			int64_t size = batch->data[1]->size;

			file.write(data, size);
		}

		file.close();



	}

	cout << "sum: " << sum << endl;

}

int gridSizeFromPointCount(uint64_t pointCount) {
	if (pointCount < 100'000'000) {
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

void savePoints(string path, vector<Vector3<double>> points) {
	int64_t sum = 0;

	auto file = std::fstream(path, std::ios::out);
	//auto file = std::fstream(path, std::ios::out | std::ios::binary);

	for (Vector3<double> &point : points) {
		
		file << point.x << ", " << point.y << ", " << point.z << endl;

	}

	file.close();
}


future<void> run() {

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	string path = "D:/dev/pointclouds/archpro/heidentor.las";
	//string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	string targetDirectory = "D:/temp/test";

	auto tStart = now();

	LASLoader* loader = new LASLoader(path);

	Metadata metadata;
	metadata.targetDirectory = targetDirectory;
	metadata.min = loader->min;
	metadata.max = loader->max;
	metadata.numPoints = loader->numPoints;
	metadata.chunkGridSize = gridSizeFromPointCount(metadata.numPoints);

	//Chunker* chunker = co_await chunking(loader, metadata);

	vector<Chunk*> chunks = getListOfChunks(metadata);
	loadChunk(chunks[0]);
	processChunk(chunks[0]);


	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	co_return;
}

future<void> runSubsampler() {

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/archpro/heidentor.las";
	string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	string targetDirectory = "D:/temp/test";

	auto tStart = now();

	LASLoader* loader = new LASLoader(path);

	Metadata metadata;
	metadata.targetDirectory = targetDirectory;
	metadata.min = loader->min;
	metadata.max = loader->max;
	metadata.numPoints = loader->numPoints;
	metadata.chunkGridSize = gridSizeFromPointCount(metadata.numPoints);

	Subsampler* subsampler = new Subsampler(targetDirectory, 0.02, metadata.min, metadata.max);

	int batchNumber = 0;
	Points* batch = co_await loader->nextBatch();
	while (batch != nullptr) {
		cout << "batch loaded: " << batchNumber << endl;

		subsampler->add(batch);

		batch = co_await loader->nextBatch();

		batchNumber++;
	}

	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	savePoints("C:/temp/subsample.txt", subsampler->getPoints());

	return;
}

future<void> runSubsampler_PoissonDisc() {

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	string path = "D:/dev/pointclouds/archpro/heidentor.las";
	//string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	string targetDirectory = "D:/temp/test";

	auto tStart = now();

	LASLoader* loader = new LASLoader(path);

	Metadata metadata;
	metadata.targetDirectory = targetDirectory;
	metadata.min = loader->min;
	metadata.max = loader->max;
	metadata.numPoints = loader->numPoints;
	metadata.chunkGridSize = gridSizeFromPointCount(metadata.numPoints);

	Subsampler_PoissonDisc* subsampler = new Subsampler_PoissonDisc(
		targetDirectory, 0.2, metadata.min, metadata.max);

	int batchNumber = 0;
	Points* batch = co_await loader->nextBatch();
	while (batch != nullptr) {
		cout << "batch loaded: " << batchNumber << endl;

		subsampler->add(batch);

		batch = co_await loader->nextBatch();

		batchNumber++;
	}

	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	savePoints("C:/temp/subsample_poisson.txt", subsampler->getPoints());

	return;
}

int main(int argc, char **argv){


	run().wait();
	//runSubsampler().wait();
	//runSubsampler_PoissonDisc().wait();
	
	return 0;
}

