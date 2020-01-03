
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <stack>

#include "ChunkProcessor.h"
#include "LASWriter.hpp"

using namespace std;

struct Cell {
	vector<Point> accepted;

};

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

class SparseGrid {

public:

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	double spacing = 1.0;
	double squaredSpacing = 1.0;

	unordered_map<uint64_t, Cell*> grid;
	uint64_t gridSize = 1;
	double gridSizeD = 1.0;

	SparseGrid(Vector3<double> min, Vector3<double> max, double spacing) {
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->spacing = spacing;
		this->squaredSpacing = spacing * spacing;

		gridSize = 5.0 * size.x / spacing;
		gridSizeD = double(gridSize);
	}

	bool isDistant(Point& candidate) {

		Vector3<uint64_t> gridCoord = toGridCoordinate(candidate);

		uint64_t xStart = std::max(gridCoord.x - 1, 0ull);
		uint64_t yStart = std::max(gridCoord.y - 1, 0ull);
		uint64_t zStart = std::max(gridCoord.z - 1, 0ull);
		uint64_t xEnd = std::min(gridCoord.x + 1, gridSize - 1);
		uint64_t yEnd = std::min(gridCoord.y + 1, gridSize - 1);
		uint64_t zEnd = std::min(gridCoord.z + 1, gridSize - 1);

		for (uint64_t x = xStart; x <= xEnd; x++) {
		for (uint64_t y = yStart; y <= yEnd; y++) {
		for (uint64_t z = zStart; z <= zEnd; z++) {

			uint64_t index = x + y * gridSize + z * gridSize * gridSize;

			auto it = grid.find(index);

			if (it == grid.end()) {
				continue;
			}

			Cell* cell = it->second;

			for (Point& alreadyAccepted : cell->accepted) {
				double dd = candidate.squaredDistanceTo(alreadyAccepted);

				if (dd < squaredSpacing) {
					return false;
				}
			}

		}
		}
		}

		return true;
	}

	void add(Point& point) {
		uint64_t index = toGridIndex(point);

		if (grid.find(index) == grid.end()) {
			Cell* cell = new Cell();
			
			grid.insert(std::make_pair(index, cell));
		}

		grid[index]->accepted.push_back(point);
	}

	Vector3<uint64_t> toGridCoordinate(Point& point) {

		double x = point.x;
		double y = point.y;
		double z = point.z;

		uint64_t ux = int32_t(gridSizeD * (x - min.x) / size.x);
		uint64_t uy = int32_t(gridSizeD * (y - min.y) / size.y);
		uint64_t uz = int32_t(gridSizeD * (z - min.z) / size.z);

		return {ux, uy, uz};
	}

	uint64_t toGridIndex(Point& point) {

		double x = point.x;
		double y = point.y;
		double z = point.z;

		uint64_t ux = int32_t(gridSizeD * (x - min.x) / size.x);
		uint64_t uy = int32_t(gridSizeD * (y - min.y) / size.y);
		uint64_t uz = int32_t(gridSizeD * (z - min.z) / size.z);

		ux = std::min(ux, gridSize - 1);
		uy = std::min(uy, gridSize - 1);
		uz = std::min(uz, gridSize - 1);

		uint64_t index = ux + gridSize * uy + gridSize * gridSize * uz;

		return index;
	}

};

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

class Node {

public:

	struct BoundingBox {
		Vector3<double> min;
		Vector3<double> max;
	};

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	double spacing = 1.0;
	vector<Node*> children = vector<Node*>(8, nullptr);
	SparseGrid* grid = nullptr;
	vector<Point> accepted;

	vector<Point> store;
	bool storeExceeded = false;
	int maxStoreSize = 1'000;

	Node(Vector3<double> min, Vector3<double> max, double spacing) {
		this->min = min;
		this->max = max;
		this->size = max - min;

		grid = new SparseGrid(min, max, spacing);
	}

	void add(Point& candidate) {

		bool isDistant = grid->isDistant(candidate);

		if (isDistant) {
			accepted.push_back(candidate);
			grid->add(candidate);
		} else if(!storeExceeded){
			store.push_back(candidate);

			if (store.size() > maxStoreSize) {
				processStore();
			}
		}else{
			addToChild(candidate);
		}

	}

	void addToChild(Point& point) {
		int childIndex = childIndexOf(point);

		if (children[childIndex] == nullptr) {
			BoundingBox box = childBoundingBoxOf(point);
			Node* child = new Node(box.min, box.max, spacing * 0.5);

			children[childIndex] = child;
		}

		children[childIndex]->add(point);
	}

	void processStore() {
		storeExceeded = true;

		for (Point& point : store) {
			addToChild(point);
		}

		store.clear();
	}

	int childIndexOf(Point& point) {
		
		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		int childIndex = 0;

		if (nx > 0.5) {
			childIndex = childIndex | 0b001;
		}

		if (ny > 0.5) {
			childIndex = childIndex | 0b010;
		}

		if (nz > 0.5) {
			childIndex = childIndex | 0b100;
		}

		return childIndex;
	}

	BoundingBox childBoundingBoxOf(Point& point) {
		BoundingBox box;
		Vector3<double> center = min + (size * 0.5);

		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		if (nx <= 0.5) {
			box.min.x = min.x;
			box.max.x = center.x;
		} else {
			box.min.x = center.x;
			box.max.x = max.x;
		}

		if (ny <= 0.5) {
			box.min.y = min.y;
			box.max.y = center.y;
		} else {
			box.min.y = center.y;
			box.max.y = max.y;
		}

		if (nz <= 0.5) {
			box.min.z = min.z;
			box.max.z = center.z;
		} else {
			box.min.z = center.z;
			box.max.z = max.z;
		}

		return box;
	}

};

void processChunk(Chunk* chunk) {

	Points* points = chunk->points;
	cout << "count: " << points->points.size() << endl;
	cout << chunk->min.toString() << endl;
	cout << chunk->max.toString() << endl;

	double spacing = 5.0;
	Node* chunkRoot = new Node(chunk->min, chunk->max, spacing);

	double tStart = now();

	for (Point& point : chunk->points->points) {

		chunkRoot->add(point);
		
	}

	auto accepted = chunkRoot->accepted;

	printElapsedTime("processing", tStart);

	LASHeader header;
	header.min = chunk->min;
	header.max = chunk->max;
	header.numPoints = accepted.size();
	header.scale = { 0.001, 0.001, 0.001 };

	//writeLAS("C:/temp/test/chunk.las", header, accepted);


	int i = 0;
	stack<Node*> todo;
	todo.push(chunkRoot);

	while (!todo.empty()) {
		Node* node = todo.top();
		todo.pop();

		
		string filename = "chunk_" + to_string(chunk->index) + "_node_" + to_string(i) + ".las";
		header.numPoints = node->accepted.size();
		writeLAS("C:/temp/test/" + filename, header, node->accepted, chunk->points);
		i++;

		//for (Node* child : node->children) {

		//	if (child == nullptr) {
		//		continue;
		//	}

		//	todo.push(child);
		//}
	}

}