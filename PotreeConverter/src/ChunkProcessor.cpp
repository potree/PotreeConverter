
#include "ChunkProcessor.h"



future<Chunk*> ChunkLoader::nextLoadedChunk() {

	auto fut = std::async(std::launch::async, [&]() -> Chunk* {

		while (true) {

			//{ // return nullptr if no more work
			//	lock_guard<mutex> lock1(mtx_chunksToLoad);
			//	lock_guard<mutex> lock2(mtx_chunksLoading);
			//	lock_guard<mutex> lock3(mtx_chunksLoaded);

			//	bool allDone = chunksToLoad.size() == 0 && chunksLoading == 0 && chunksLoaded.size() == 0;

			//	if (allDone) {
			//		return nullptr;
			//	}
			//}

			//{ // return nullptr if no more work
			if (isDone()) {
				return nullptr;
			}

			// otherwise, return a loaded chunk, if available, or wait otherwise
			{
				lock_guard<mutex> lock(mtx_chunksLoaded);

				if (chunksLoaded.size() > 0) {
					Chunk* chunk = chunksLoaded.back();
					chunksLoaded.pop_back();

					return chunk;
				} else {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}

		}

	});

	return fut;
}

void ChunkLoader::loadChunk(Chunk* chunk) {
	auto filesize = fs::file_size(chunk->file);
	int numPoints = filesize / 28;

	Attribute aPosition;
	aPosition.byteOffset = 0;
	aPosition.bytes = 24;
	aPosition.name = "position";
	Buffer* posData = new Buffer(numPoints * aPosition.bytes);

	Attribute aColor;
	aColor.byteOffset = 12;
	aColor.bytes = 4;
	aColor.name = "color";
	Buffer* colorData = new Buffer(numPoints * aColor.bytes);

	Points* points = new Points();
	points->count = numPoints;
	points->attributes.push_back(aPosition);
	points->attributes.push_back(aColor);
	points->data.push_back(posData);
	points->data.push_back(colorData);


	auto file = fstream(chunk->file, std::ios::in | std::ios::binary);

	file.read(posData->dataChar, posData->size);
	file.read(colorData->dataChar, colorData->size);

	file.close();

	chunk->points = points;
}

void ChunkLoader::spawnThreads(int numThreads) {

	for(int i = 0; i < numThreads; i++){

		thread t([&](){

			while (true) {
				//printThreadsafe("loader next round");

				Chunk* chunk = nullptr;

				{ // obtain workload or leave loop if done
					lock_guard<mutex> lock1(mtx_chunksToLoad);
					lock_guard<mutex> lock2(mtx_chunksLoading);

					if (chunksToLoad.size() == 0) {
						break;
					} else {
						chunk = chunksToLoad.back();
						chunksToLoad.pop_back();

						chunksLoading++;
					}
				}

				//cout << "loading chunk " << chunk->index << endl;
				printThreadsafe("loading chunk ", std::to_string(chunk->index));
				loadChunk(chunk);
				printThreadsafe("loaded chunk ", std::to_string(chunk->index));
				//cout << "loaded chunk " << chunk->index << endl;

				{
					lock_guard<mutex> lock1(mtx_chunksLoaded);
					lock_guard<mutex> lock2(mtx_chunksLoading);

					chunksLoading--;
					chunksLoaded.push_back(chunk);
				}

				//printThreadsafe("loader sleep");
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

		});

		t.detach();
	}

}


vector<Chunk*> ChunkProcessor::listOfChunksToProcess() {
	string chunkDirectory = metadata.targetDirectory + "/chunks";

	auto toIndex = [](string filename) -> int {
		string strID = stringReplace(filename, "chunk_", "");
		strID = stringReplace(strID, ".bin", "");

		int index = std::stoi(strID);

		return index;
	};

	int chunkGridSize = metadata.chunkGridSize;
	auto toCellCoordinate = [chunkGridSize](int index) -> Vector3<int> {
		int size = chunkGridSize;
		int x = index % size;
		int y = (index % (size * size) - x) / size;
		int z = (index - (x + y * size)) / (size * size);

		Vector3<int> coordinates = { x, y, z };

		return coordinates;
	};

	vector<Chunk*> chunksToLoad;
	for (const auto& entry : fs::directory_iterator(chunkDirectory)) {

		string filename = entry.path().filename().string();
		int chunkIndex = toIndex(filename);
		Vector3<int> cellCoordinate = toCellCoordinate(chunkIndex);

		Chunk* chunk = new Chunk();
		chunk->file = entry.path().string();
		chunk->index = chunkIndex;
		chunk->index3D = cellCoordinate;

		chunksToLoad.push_back(chunk);
	}

	return chunksToLoad;
}

void ChunkWriter::writeChunk(Chunk* chunk) {

}