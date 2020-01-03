
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <iterator>

#include "ChunkProcessor.h"
#include "LASWriter.hpp"

double squaredDistance(Point& a, Point& b) {
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	double dz = b.z - a.z;

	double dd = dx * dx + dy * dy + dz * dz;

	return dd;
};

struct GridOfAccepted {

	struct GridIndex {
		int ix = 0;
		int iy = 0;
		int iz = 0;
		int index = 0;
	};

	int gridSize = 0;
	//vector<vector<Point>> grid;
	unordered_map<int, vector<Point>> grid;
	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	double spacing = 0.0;
	double squaredSpacing = 0.0;

	int maxChecks = 0;

	GridOfAccepted(int gridSize, Vector3<double> min, Vector3<double> max, double spacing) {
		this->gridSize = gridSize;
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->spacing = spacing;
		this->squaredSpacing = spacing * spacing;

		//grid.resize(gridSize * gridSize * gridSize);
	}

	GridIndex toGridIndex(Point& point) {
		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		int ux = double(gridSize) * nx;
		int uy = double(gridSize) * ny;
		int uz = double(gridSize) * nz;

		ux = std::min(ux, gridSize - 1);
		uy = std::min(uy, gridSize - 1);
		uz = std::min(uz, gridSize - 1);

		int index = ux + uy * gridSize + uz * gridSize * gridSize;

		return { ux, uy, uz, index };
	}

	void add(Point& point) {

		GridIndex index = toGridIndex(point);

		grid[index.index].push_back(point);
	}

	bool check(Point& candidate, int index, int& checks) {
		vector<Point>& cell = grid[index];
		//for (Point& point : cell) {

		//	double dd = squaredDistance(point, candidate);

		//	checks++;

		//	if (dd < squaredSpacing) {
		//		return false;
		//	}
		//}

		for (int i = cell.size() - 1; i >= 0; i--) {
			Point& point = cell[i];
			double dd = squaredDistance(point, candidate);

			checks++;

			if (candidate.x - point.x > spacing) {
				return true;
			}
	
			if (dd < squaredSpacing) {
				return false;
			}
		}

		return true;
	}

	bool isDistant(Point candidate, int& checks) {

		GridIndex candidateIndex = toGridIndex(candidate);

		int xEnd = std::max(candidateIndex.ix - 1, 0);
		int yEnd = std::max(candidateIndex.iy - 1, 0);
		int zEnd = std::max(candidateIndex.iz - 1, 0);
		int xStart = std::min(candidateIndex.ix + 0, gridSize - 1);
		int yStart = std::min(candidateIndex.iy + 1, gridSize - 1);
		int zStart = std::min(candidateIndex.iz + 1, gridSize - 1);

		{ // check candidate cell first
			bool c = check(candidate, candidateIndex.index, checks);

			if (!c) {
				return false;
			}
		}

		// then check cells in neighborhood
		for (int ix = xStart; ix >= xEnd; ix--) {
			for (int iy = yStart; iy >= yEnd; iy--) {
				for (int iz = zStart; iz >= zEnd; iz--) {
					int index = ix + iy * gridSize + iz * gridSize * gridSize;

					if (index == candidateIndex.index) {
						continue;
					}

					bool c = check(candidate, index, checks);

					if (!c) {
						return false;
					}
				}
			}
		}

		return true;
	}

};


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
	Vector3<double> cubeSize = {cubeSizeScalar , cubeSizeScalar , cubeSizeScalar};
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

int maxChecks = 0;

vector<Point> subsample(vector<Point>& candidates, double spacing, GridOfAccepted& gridOfAccepted) {

	vector<Point> accepted;

	int numPoints = candidates.size();
	for (int i = 0; i < numPoints; i++) {

		int checks = 0;

		Point& candidate = candidates[i];
		bool distant = gridOfAccepted.isDistant(candidate, checks);

		maxChecks = std::max(maxChecks, checks);

		if (distant) {
			accepted.push_back(candidate);
			gridOfAccepted.add(candidate);
		}

	}

	return accepted;
}

vector<Points*> split(Points* input, int gridSize, Vector3<double> min, Vector3<double> max) {

	struct Cell {

		Points* points = nullptr;

		int32_t ux = 0;
		int32_t uy = 0;
		int32_t uz = 0;
	};

	Vector3<double> size = max - min;
	double gridSizeD = double(gridSize);
	vector<Cell*> grid(gridSize, nullptr);

	for (int i = 0; i < input->points.size(); i++) {

		Point point = input->points[i];

		int32_t ux = int32_t(gridSizeD * (point.x - min.x) / size.x);
		int32_t index = std::min(ux, gridSize - 1);

		Cell* cell = grid[index];

		if (cell == nullptr) {
			cell = new Cell();
			cell->points = new Points();
			cell->ux = ux;
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

bool sortByX(Point& a, Point& b) {
	return a.x < b.x;
}

vector<Points*> makeBins(Chunk* chunk) {
	
	double tStartBinning = now();

	Points* points = chunk->points;
	int numPoints = points->points.size();

	//int gridSize = 0;

	//if (numPoints > 10'000'000) {
	//	gridSize = 1;
	//} else if (numPoints > 4'000'000) {
	//	gridSize = 5;
	//} else if (numPoints > 1'000'000) {
	//	gridSize = 4;
	//} else if (numPoints > 400'000) {
	//	gridSize = 3;
	//} else if (numPoints > 100'000) {
	//	gridSize = 2;
	//}

	int gridSize = 100;

	vector<Points*> bins;

	if (numPoints < 100'000) {
		bins = { points };
	} else {
		Vector3<double> min = chunk->min;
		Vector3<double> max = chunk->max;
		bins = split(points, gridSize, min, max);
	}

	printElapsedTime("binning", tStartBinning);
	cout << "#bins" << bins.size() << endl;

	return bins;
}

void processChunk(Chunk* chunk) {

	Points* points = chunk->points;
	cout << "count: " << points->points.size() << endl;
	cout << chunk->min.toString() << endl;
	cout << chunk->max.toString() << endl;


	double tStart = now();



	// BINNING
	vector<Points*> bins = makeBins(chunk);

	double spacing = 1;
	
	double tStartProcessing = now();

	GridOfAccepted acceptedL0(128, chunk->min, chunk->max, spacing);

	printElapsedTime("create accepted grid", tStartProcessing);
	
	vector<Point> accepted;
	for (Points* bin : bins) {

		std::sort(bin->points.begin(), bin->points.end(), sortByX);

		vector<Point> sampleAccepted = subsample(bin->points, spacing, acceptedL0);
		accepted.insert(accepted.end(), sampleAccepted.begin(), sampleAccepted.end());
	}

	printElapsedTime("processing", tStartProcessing);
	cout << "#accepted: " << accepted.size() << endl;

	cout << "maxChecks: " << maxChecks << endl;





	// DEBUG / WRITE FILES



	LASHeader header;
	header.min = chunk->min;
	header.max = chunk->max;
	header.numPoints = accepted.size();
	header.scale = { 0.001, 0.001, 0.001 };

	{
		double tStartWriting = now();

		writeLAS("C:/temp/level_0.las", header, accepted, points);

		printElapsedTime("writing file", tStartWriting);
	}

	return;

	int numLevels = 6;
	vector<Point> toSample = accepted;

	for (int i = 0; i < numLevels; i++) {
		int gridSize = acceptedL0.gridSize / pow(2, i + 1);
		double spacing = acceptedL0.spacing * pow(2.0, i + 1);

		GridOfAccepted acceptedLX(gridSize, chunk->min, chunk->max, spacing);

		auto levelX = subsample(toSample, spacing, acceptedLX);

		header.numPoints = levelX.size();
		string filename = "level_" + to_string(i + 1) + ".las";
		writeLAS("C:/temp/" + filename, header, levelX, points);

		toSample = levelX;
	}
}