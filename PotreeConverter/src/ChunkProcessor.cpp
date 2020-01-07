
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

vector<Chunk*> getListOfChunks(Metadata& metadata) {
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

	int gridSize = metadata.chunkGridSize;
	//Vector3<double> min = metadata.min;
	//Vector3<double> max = metadata.max;
	Vector3<double> fittedSize = metadata.max - metadata.min;
	double cubeSizeScalar = std::max(std::max(fittedSize.x, fittedSize.y), fittedSize.z);
	Vector3<double> cubeSize = { cubeSizeScalar , cubeSizeScalar , cubeSizeScalar };
	Vector3<double> min = metadata.min;
	Vector3<double> max = min + cubeSize;
	Vector3<double> cellSize = cubeSize / double(gridSize);

	vector<Chunk*> chunksToLoad;
	for (const auto& entry : fs::directory_iterator(chunkDirectory)) {

		string filename = entry.path().filename().string();
		int chunkIndex = toIndex(filename);
		Vector3<int> cellCoordinate = toCellCoordinate(chunkIndex);

		Chunk* chunk = new Chunk();
		chunk->file = entry.path().string();
		chunk->index = chunkIndex;
		chunk->index3D = cellCoordinate;

		Vector3<double> dcoord = {
			double(cellCoordinate.x),
			double(cellCoordinate.y),
			double(cellCoordinate.z)
		};
		Vector3<double> cellMin = min + (dcoord * cellSize);
		Vector3<double> cellMax = cellMin + cellSize;

		chunk->min = cellMin;
		chunk->max = cellMax;

		chunksToLoad.push_back(chunk);
	}

	return chunksToLoad;
}

void loadChunk(Chunk* chunk) {
	auto filesize = fs::file_size(chunk->file);
	int numPoints = filesize / 28;

	Attribute aColor;
	aColor.byteOffset = 0;
	aColor.bytes = 4;
	aColor.name = "color";

	Points* points = new Points();
	points->attributes.push_back(aColor);
	points->attributeBuffer = new Buffer(numPoints * aColor.bytes);

	auto file = fstream(chunk->file, std::ios::in | std::ios::binary);


	ifstream inputFile("shorts.txt", std::ios::binary);

	int bufferSize = numPoints * sizeof(Vector3<double>);
	void* buffer = malloc(bufferSize);
	Vector3<double>* coordinates = reinterpret_cast<Vector3<double>*>(buffer);
	file.read(reinterpret_cast<char*>(coordinates), bufferSize);
	file.read(points->attributeBuffer->dataChar, points->attributeBuffer->size);

	for (int i = 0; i < numPoints; i++) {

		Point point;
		point.x = coordinates[i].x;
		point.y = coordinates[i].y;
		point.z = coordinates[i].z;

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


Node* processChunk(Chunk* chunk) {

	Points* points = chunk->points;
	//cout << "count: " << points->points.size() << endl;
	//cout << chunk->min.toString() << endl;
	//cout << chunk->max.toString() << endl;

	double spacing = 5.0;
	Node* chunkRoot = new Node(chunk->min, chunk->max, spacing);
	chunkRoot->name = "r";

	double tStart = now();

	for (Point& point : chunk->points->points) {

		chunkRoot->add(point);
		
	}

	auto accepted = chunkRoot->accepted;

	printElapsedTime("processing", tStart);

	return chunkRoot;
	//writePotree("C:/dev/test/potree", chunk, chunkRoot);

	// write root
	//{
	//	LASHeader header;
	//	header.min = chunk->min;
	//	header.max = chunk->max;
	//	header.numPoints = accepted.size();
	//	header.scale = { 0.001, 0.001, 0.001 };
	//	header.numPoints = chunkRoot->accepted.size();

	//	string filename = "chunk_" + to_string(chunk->index) + "_node_r.las";
	//	writeLAS("C:/temp/test/" + filename, header, chunkRoot->accepted, chunk->points);
	//}

}