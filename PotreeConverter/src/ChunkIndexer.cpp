
#include "ChunkProcessor.h"

#include <ostream>
#include <unordered_map>

using std::unordered_map;



void ChunkIndexer::indexChunk(Chunk* chunk) {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	Points* points = chunk->points;
	Buffer* bPosition = points->data[0];
	double* positions = bPosition->dataD;

	Vector3<double> min = metadata.min;
	Vector3<double> max = metadata.max;
	Vector3<double> size = max - min;

	int32_t gridSize = 512;
	//vector<int> cells(gridSize * gridSize * gridSize);
	unordered_map<int32_t, Vector3<double>> cellsMap;

	for (int i = 0; i < points->count; i++) {
		double x = positions[3 * i + 0];
		double y = positions[3 * i + 1];
		double z = positions[3 * i + 2];

		int32_t ux = gridSize * ((x - min.x) / size.x);
		int32_t uy = gridSize * ((y - min.y) / size.y);
		int32_t uz = gridSize * ((z - min.z) / size.z);

		ux = std::min(ux, gridSize - 1);
		uy = std::min(uy, gridSize - 1);
		uz = std::min(uz, gridSize - 1);

		int32_t index = ux + uy * gridSize + uz * gridSize * gridSize;

		//cells[index]++;

		if (cellsMap.find(index) == cellsMap.end()) {
			//cellsMap.insert(index, 0);
			cellsMap[index] = {x, y, z};
		}

		//cellsMap[index]++;
	}


	string dir = chunk->file + "/../../subsampled";
	fs::create_directories(dir);

	string filename = fs::path(chunk->file).filename().string();

	string file = dir + "/" + filename + ".txt";

	std::ofstream myfile;
	myfile.open(file);

	for (auto entry : cellsMap) {
		auto point = entry.second;

		myfile << point.x << ", " << point.y << ", " << point.z << endl;

	}

	myfile.close();

}

void ChunkIndexer::spawnThreads(int numThreads) {

	for (int i = 0; i < numThreads; i++) {
		numActiveThreads++;

		thread t([&]() {

			while (true) {

				{ // quit if no more work
					printThreadsafe("check if there is more");
					lock_guard<mutex> lock1(loader->mtx_chunksLoaded);
					lock_guard<mutex> lock2(loader->mtx_chunksLoading);
					lock_guard<mutex> lock3(loader->mtx_chunksToLoad);

					bool allDone = loader->chunksLoaded.size() == 0
						&& loader->chunksLoading == 0
						&& loader->chunksToLoad.size() == 0;

					if (allDone) {
						printThreadsafe("everything indexed!");
						break;
					}
				}

				// check if something to do right now
				Chunk* chunk = nullptr;
				{
					printThreadsafe("check todo");
					lock_guard<mutex> lock1(loader->mtx_chunksLoaded);

					if (loader->chunksLoaded.size() > 0) {
						chunk = loader->chunksLoaded.back();
						loader->chunksLoaded.pop_back();
					}
					printThreadsafe("checked todo");
				}

				// do work, or sleep a little and try to check for work again
				if (chunk != nullptr) {
					// process

					printThreadsafe("index chunk ", std::to_string(chunk->index));
					indexChunk(chunk);
					printThreadsafe("indexed chunk ", std::to_string(chunk->index));

				} else {
					// sleep
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}


			}

			printThreadsafe("indexer thread done");

			numActiveThreads--;
			});

		t.detach();
	}

}