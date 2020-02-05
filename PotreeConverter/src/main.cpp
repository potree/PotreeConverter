
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include <fstream>

#include <filesystem>

#include "Metadata.h"
#include "LASLoader.hpp"
//#include "Chunker_Tree.h"
#include "Chunker2.h"
#include "Vector3.h"
#include "ChunkProcessor.h"
#include "PotreeWriter.h"
#include "ThreadPool/ThreadPool.h"

future<Chunker*> chunking(LASLoader* loader, Metadata metadata) {

	double tStart = now();

	string path = metadata.targetDirectory + "/chunks/";
	for (const auto& entry : std::filesystem::directory_iterator(path)){
		std::filesystem::remove(entry);
	}

	Vector3<double> size = metadata.max - metadata.min;
	double cubeSize = std::max(std::max(size.x, size.y), size.z);
	Vector3<double> cubeMin = metadata.min;
	Vector3<double> cubeMax = cubeMin + cubeSize;

	Attributes attributes = loader->getAttributes();
	int gridSize = metadata.chunkGridSize;
	Chunker* chunker = new Chunker(path, attributes, cubeMin, cubeMax, gridSize);

	double sum = 0.0;
	int batchNumber = 0;
	auto batch = co_await loader->nextBatch();
	while (batch != nullptr) {
		if ((batchNumber % 10) == 0) {
			cout << "batch loaded: " << batchNumber << endl;
		}

		auto tStart = now();
		chunker->add(batch);
		auto duration = now() - tStart;
		sum += duration;

		batch = co_await loader->nextBatch();

		batchNumber++;
	}

	cout << "raw batch add time: " << sum << endl;

	printElapsedTime("chunking duration", tStart);

	chunker->close();

	printElapsedTime("chunking duration + close", tStart);

	return chunker;
}




future<void> run() {

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/Riegl/niederweiden.las";
	//string path = "D:/dev/pointclouds/archpro/heidentor.las";
	//string path = "D:/dev/pointclouds/pix4d/eclepens.las";
	string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/open_topography/ca13/morro_rock/merged.las";
	//string targetDirectory = "C:/temp/test";

	string targetDirectory = "C:/dev/workspaces/Potree2/master/pointclouds/test";

	auto tStart = now();

	LASLoader* loader = new LASLoader(path);
	loader->spawnLoadThread();
	Attributes attributes = loader->getAttributes();

	auto size = loader->max - loader->min;
	double octreeSize = size.max();

	fs::create_directories(targetDirectory);
	fs::create_directories(targetDirectory + "/chunks");

	Metadata metadata;
	metadata.targetDirectory = targetDirectory;
	metadata.min = loader->min;
	metadata.max = loader->min + octreeSize;
	metadata.numPoints = loader->numPoints;
	metadata.chunkGridSize = 8;
	int upperLevels = 3;

	Chunker* chunker = co_await chunking(loader, metadata);

	{
		vector<shared_ptr<Chunk>> chunks = getListOfChunks(metadata);
		//chunks.resize(39);
		//chunks = {chunks[38]};

		double scale = 0.001;
		double spacing = loader->min.distanceTo(loader->max) / 100.0;
		PotreeWriter writer(targetDirectory,
			metadata.min,
			metadata.max,
			spacing,
			scale,
			upperLevels,
			chunks,
			attributes
		);

		auto min = metadata.min;
		auto max = metadata.max;

		// parallel
		ThreadPool* pool = new ThreadPool(16);
		for (int i = 0; i < chunks.size(); i++) {

			shared_ptr<Chunk> chunk = chunks[i];


			//auto usage1 = getMemoryUsage();

			//{
			//	auto points = loadChunk(chunk, attributes);
			//	//ProcessResult processResult = processChunk(chunk, points, min, max, spacing);
			//	//writer.writeChunk(chunk, points, processResult);
			//}
			//

			//auto usage2 = getMemoryUsage();

			//cout << usage1.usedMemory << endl;
			//cout << usage2.usedMemory << endl;

			pool->enqueue([chunk, attributes, &writer, min, max, spacing]() {
				auto points = loadChunk(chunk, attributes);

				ProcessResult processResult = processChunk(chunk, points, min, max, spacing);

				writer.writeChunk(chunk, points, processResult);
			});
		}
		delete pool;

		writer.close();

	}

	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	co_return;
}

int main(int argc, char **argv){

	run().wait();

	return 0;
}

