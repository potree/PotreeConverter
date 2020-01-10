
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <stack>

#include "ChunkProcessor.h"
#include "LASWriter.hpp"

using namespace std;

uint64_t toGridIndex(Chunk* chunk, Point& point, uint64_t gridSize) {

	double x = point.x;
	double y = point.y;
	double z = point.z;

	double gridSizeD = double(gridSize);
	Vector3<double>& min = chunk->min;
	Vector3<double> size = chunk->max - chunk->min;

	uint64_t ux = int32_t(gridSizeD * (x - min.x) / size.x);
	uint64_t uy = int32_t(gridSizeD * (y - min.y) / size.y);
	uint64_t uz = int32_t(gridSizeD * (z - min.z) / size.z);

	ux = std::min(ux, gridSize - 1);
	uy = std::min(uy, gridSize - 1);
	uz = std::min(uz, gridSize - 1);

	uint64_t index = ux + gridSize * uy + gridSize * gridSize * uz;

	return index;
}


struct BoundingBox {
	Vector3<double> min;
	Vector3<double> max;
};

BoundingBox childBoundingBoxOf(BoundingBox box, int index) {
	Vector3<double> center = (box.max + box.min) / 2.0;
	BoundingBox childBox;

	if ((index & 0b100) == 0) {
		childBox.min.x = box.min.x;
		childBox.max.x = center.x;
	} else {
		childBox.min.x = center.x;
		childBox.max.x = box.max.x;
	}

	if ((index & 0b010) == 0) {
		childBox.min.y = box.min.y;
		childBox.max.y = center.y;
	} else {
		childBox.min.y = center.y;
		childBox.max.y = box.max.y;
	}

	if ((index & 0b01) == 0) {
		childBox.min.z = box.min.z;
		childBox.max.z = center.z;
	} else {
		childBox.min.z = center.z;
		childBox.max.z = box.max.z;
	}

	return childBox;
}

vector<Chunk*> getListOfChunks(Metadata& metadata) {
	string chunkDirectory = metadata.targetDirectory + "/chunks";

	auto toID = [](string filename) -> string {
		string strID = stringReplace(filename, "chunk_", "");
		strID = stringReplace(strID, ".bin", "");

		return strID;
	};

	vector<Chunk*> chunksToLoad;
	for (const auto& entry : fs::directory_iterator(chunkDirectory)) {
		string filename = entry.path().filename().string();
		string chunkID = toID(filename);

		if (!iEndsWith(filename, ".bin")) {
			continue;
		}

		Chunk* chunk = new Chunk();
		chunk->file = entry.path().string();
		chunk->id = chunkID;

		Vector3<double> min = metadata.min;
		Vector3<double> max = metadata.max;
		BoundingBox box = { min, max };

		for (int i = 1; i < chunkID.size(); i++) {
			int index = chunkID[i] - '0'; // this feels so wrong...

			box = childBoundingBoxOf(box, index);	
		}

		chunk->min = box.min;
		chunk->max = box.max;

		chunksToLoad.push_back(chunk);
	}

	return chunksToLoad;
}

void loadChunk(Chunk* chunk, Attributes attributes) {
	auto filesize = fs::file_size(chunk->file);
	uint64_t bytesPerPoint = 28;
	int numPoints = filesize / bytesPerPoint;

	uint64_t attributeBufferSize = numPoints * attributes.byteSize;

	Points* points = new Points();
	points->attributes = attributes;
	points->attributeBuffer = new Buffer(attributeBufferSize);

	int attributesByteSize = 4;

	auto file = fstream(chunk->file, std::ios::in | std::ios::binary);


	ifstream inputFile("shorts.txt", std::ios::binary);

	int bufferSize = numPoints * bytesPerPoint;
	void* buffer = malloc(bufferSize);
	uint8_t* bufferU8 = reinterpret_cast<uint8_t*>(buffer);
	
	file.read(reinterpret_cast<char*>(buffer), bufferSize);

	for (uint64_t i = 0; i < numPoints; i++) {

		uint64_t pointOffset = i * bytesPerPoint;
		double* coordinates = reinterpret_cast<double*>(bufferU8 + pointOffset);

		Point point;
		point.x = coordinates[0];
		point.y = coordinates[1];
		point.z = coordinates[2];

		uint8_t* attSrc = bufferU8 + (bytesPerPoint * i + 24);
		uint8_t* attDest = points->attributeBuffer->dataU8 + (4 * i);
		memcpy(attDest, attSrc, attributesByteSize);

		point.index = i;

		points->points.push_back(point);
	}

	free(buffer);

	file.close();

	chunk->points = points;
}



vector<Points*> split(Chunk* chunk, Points* input, int gridSize) {
	struct Cell {

		Points* points;

		int32_t ux = 0;
		int32_t uy = 0;
		int32_t uz = 0;
	};

	auto min = chunk->min;
	auto max = chunk->max;
	auto size = max - min;
	double gridSizeD = double(gridSize);
	vector<Cell*> grid(gridSize * gridSize * gridSize, nullptr);

	for (int i = 0; i < input->points.size(); i++) {

		Point point = input->points[i];

		uint64_t index = toGridIndex(chunk, point, gridSize);

		Cell* cell = grid[index];

		if (cell == nullptr) {
			cell = new Cell();
			cell->points = new Points();
			cell->points->attributes = input->attributes;
			grid[index] = cell;
		}

		cell->points->points.push_back(point);
	}

	vector<Points*> pointCells;
	for (Cell* cell : grid) {
		if (cell != nullptr) {
			pointCells.push_back(cell->points);
		}
	}

	return pointCells;
}


Node* processChunk(Chunk* chunk, double spacing) {
	
	Node* chunkRoot = new Node(chunk->min, chunk->max, spacing);
	chunkRoot->name = chunk->id;

	double tStart = now();

	for (Point& point : chunk->points->points) {
		chunkRoot->add(point);
	}

	printElapsedTime("processing", tStart);

	return chunkRoot;
}