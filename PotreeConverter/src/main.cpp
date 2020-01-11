
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
#include "PotreeWriter.h"
#include "ThreadPool/ThreadPool.h"

using namespace std::experimental;

namespace fs = std::experimental::filesystem;

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

	double tStart = now();

	string path = metadata.targetDirectory + "/chunks";
	for (const auto& entry : fs::directory_iterator(path)){
		fs::remove(entry);
	}

	Vector3<double> size = metadata.max - metadata.min;
	double cubeSize = std::max(std::max(size.x, size.y), size.z);
	Vector3<double> cubeMin = metadata.min;
	Vector3<double> cubeMax = cubeMin + cubeSize;

	Attributes attributes = loader->getAttributes();
	Chunker* chunker = new Chunker(path, attributes, cubeMin, cubeMax, 8);

	int batchNumber = 0;
	auto batch = co_await loader->nextBatch();
	while (batch != nullptr) {
		if ((batchNumber % 10) == 0) {
			cout << "batch loaded: " << batchNumber << endl;
		}

		chunker->add(batch);

		batch = co_await loader->nextBatch();

		batchNumber++;
	}

	chunker->close();

	printElapsedTime("chunking duration", tStart);

	return chunker;
}


future<void> run() {

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/Riegl/niederweiden.las";
	string path = "D:/dev/pointclouds/archpro/heidentor.las";
	//string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/open_topography/ca13/morro_rock/merged.las";
	//string targetDirectory = "C:/temp/test";

	string targetDirectory = "C:/dev/workspaces/potree/develop/test/new_format";

	auto tStart = now();

	LASLoader* loader = new LASLoader(path);
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
	//metadata.chunkGridSize = gridSizeFromPointCount(metadata.numPoints);

	int upperLevels = 3;
	metadata.chunkGridSize = pow(2, upperLevels);

	Chunker* chunker = co_await chunking(loader, metadata);



	vector<Chunk*> chunks = getListOfChunks(metadata);
	//chunks.resize(2);

	double scale = 0.001;
	double spacing = loader->min.distanceTo(loader->max) / 200.0;
	PotreeWriter writer(targetDirectory, 
		metadata.min,
		metadata.max,
		spacing,
		scale,
		upperLevels,
		chunks
	);
	double cSpacing = spacing / 8.0;

	//{ // sequential
	//	for (Chunk* chunk : chunks) {
	//		loadChunk(chunk);
	//	}

	//	vector<Node*> chunkRoots;
	//	for (Chunk* chunk : chunks) {
	//		Node* chunkRoot = processChunk(chunk, cSpacing);
	//		chunkRoots.push_back(chunkRoot);

	//	}

	//	for (int i = 0; i < chunks.size(); i++) {
	//		Chunk* chunk = chunks[i];
	//		Node* chunkRoot = chunkRoots[i];

	//		writer.writeChunk(chunk, chunkRoot);
	//	}
	//}


	// parallel
	ThreadPool* pool = new ThreadPool(16);
	for(int i = 0; i < chunks.size(); i++){

		Chunk* chunk = chunks[i];

		pool->enqueue([chunk, attributes, &writer, cSpacing](){
			loadChunk(chunk, attributes);

			Node* chunkRoot = processChunk(chunk, cSpacing);

			writer.writeChunk(chunk, chunkRoot);
		});
	}
	delete pool;

	writer.close();

	


	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	co_return;
}

//#include "TaskPool.h"

//void testTaskPool() {
//
//	struct Batch {
//		string path = "";
//		string text = "";
//
//		Batch(string path, string text) {
//			this->path = path;
//			this->text = text;
//		}
//	};
//
//	string someCapturedValue = "asoudh adpif sdgsrg";
//	auto processor = [someCapturedValue](shared_ptr<Batch> batch) {
//		fstream file;
//		file.open(batch->path, ios::out);
//		file << batch->text;
//		file << someCapturedValue;
//		file.close();
//	};
//
//	TaskPool<Batch> pool(5, processor);
//
//	shared_ptr<Batch> batch1 = make_shared<Batch>(
//		"C:/temp/test1.txt",
//		"content of file 1 ");
//	shared_ptr<Batch> batch2 = make_shared<Batch>(
//		"C:/temp/test2.txt",
//		"content of file 2 ");
//
//	pool.addTask(batch1);
//	pool.addTask(batch2);
//
//	pool.close();
//}

int main(int argc, char **argv){

	run().wait();

	//testTaskPool();

	return 0;
}

