
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include <fstream>

#include <filesystem>

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
	chunker->min = metadata.min;
	chunker->max = metadata.max;

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

	ChunkProcessor* processor = new ChunkProcessor(metadata);
	processor->waitFinished();

	std::this_thread::sleep_for(std::chrono::milliseconds(500));




	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

	co_return;
}

void threadTest() {

	auto tStart = now();

	vector<thread> threads;

	for (int i = 0; i < 10; i++) {

		threads.push_back(
			thread([i](){
				std::this_thread::sleep_for(std::chrono::seconds(1));

				auto id = std::this_thread::get_id();

				cout << "thread: " << i << ", id: " << id << endl;
			})
		);

	}

	cout << "spawned" << endl;

	for (int i = 0; i < threads.size(); i++) {
		threads[i].join();
	}

	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;
}

void futureTest() {

	auto tStart = now();

	vector<future<void>> futures;

	auto launch = std::launch::async;
	auto task = [](int value) {
		std::this_thread::sleep_for(std::chrono::seconds(1));

		auto id = std::this_thread::get_id();

		cout << "thread: " << value << ", id: " << id << endl;
	};

	for (int i = 0; i < 10; i++) {
		futures.push_back(async(launch, task, i));
	}

	cout << "spawned" << endl;

	for (int i = 0; i < futures.size(); i++) {
		futures[i].wait();
	}
	
	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;



}



int main(int argc, char **argv){

	//futureTest();

	//threadTest();

	run().wait();
	
	return 0;
}

