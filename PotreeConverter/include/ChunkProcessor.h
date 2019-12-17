
#pragma once

#include <string>
#include <filesystem>

#include "Metadata.h"

using std::string;

namespace fs = std::experimental::filesystem;

struct Chunk {

	string file = "";
	Points* points = nullptr;
	int index = -1;
	Vector3<int> index3D;

	Chunk() {

	}
};

class ChunkProcessor {
public:

	Metadata metadata;

	vector<Chunk> chunks;

	ChunkProcessor(Metadata metadata) {
		this->metadata = metadata;

		initialize();
	}

	void initialize() {

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

		for (const auto& entry : fs::directory_iterator(chunkDirectory)){

			string filename = entry.path().filename().string();
			int chunkIndex = toIndex(filename);
			Vector3<int> cellCoordinate = toCellCoordinate(chunkIndex);

			Chunk chunk;
			chunk.file = entry.path().string();
			chunk.index = chunkIndex;
			chunk.index3D = cellCoordinate;

			chunks.push_back(chunk);
		}

		spawnChunkLoader();

	}

	void spawnChunkLoader() {

		for (Chunk& chunk : chunks) {
			//cout << chunk.file << endl;



		}

	}

	void spawnChunkProcessor() {

	}

};