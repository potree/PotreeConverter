
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <iterator>

#include "ChunkProcessor.h"

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
	vector<vector<Point>> grid;
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

		grid.resize(gridSize * gridSize * gridSize);
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

	bool check(Point& candidate, int index) {
		vector<Point>& cell = grid[index];
		for (Point& point : cell) {

			double dd = squaredDistance(point, candidate);

			if (dd < squaredSpacing) {
				return false;
			}
		}

		//for (int i = cell.size() - 1; i >= 0; i--) {
		//	Point& point = cell[i];
		//	double dd = squaredDistance(point, candidate);

		//	if (candidate.x - point.x > spacing) {
		//		return true;
		//	}
	
		//	if (dd < squaredSpacing) {
		//		return false;
		//	}
		//}

		return true;
	}

	bool isDistant(Point candidate) {

		int checks = 0;

		GridIndex candidateIndex = toGridIndex(candidate);

		/*int xStart = std::max(index.ix - 1, 0);
		int yStart = std::max(index.iy - 1, 0);
		int zStart = std::max(index.iz - 1, 0);
		int xEnd = std::min(index.ix + 0, gridSize - 1);
		int yEnd = std::min(index.iy + 1, gridSize - 1);
		int zEnd = std::min(index.iz + 1, gridSize - 1);*/

		int xEnd = std::max(candidateIndex.ix - 1, 0);
		int yEnd = std::max(candidateIndex.iy - 1, 0);
		int zEnd = std::max(candidateIndex.iz - 1, 0);
		int xStart = std::min(candidateIndex.ix + 0, gridSize - 1);
		int yStart = std::min(candidateIndex.iy + 1, gridSize - 1);
		int zStart = std::min(candidateIndex.iz + 1, gridSize - 1);

		//int xEnd = index.ix;
		//int yEnd = index.iy;
		//int zEnd = index.iz;

		{ // check candidate cell first
			bool c = check(candidate, candidateIndex.index);

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

					bool c = check(candidate, index);

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

		points->points.push_back(point);
	}

	free(buffer);

	file.close();

	chunk->points = points;
}

const double SQRT_2 = sqrt(2.0);

//bool compare(Point& a, Point& b) {
//
//	auto da = (a.x + a.y + a.z) / SQRT_2;
//	auto db = (b.x + b.y + b.z) / SQRT_2;
//
//	return da < db;
//}

bool compare(Point& a, Point& b) {

	return a.x < b.x;
}




vector<Point> subsample(vector<Point>& candidates, double spacing, GridOfAccepted& gridOfAccepted) {

	//double squaredSpacing = spacing * spacing;
	vector<Point> accepted;

	int numPoints = candidates.size();
	for (int i = 0; i < numPoints; i++) {

		Point& candidate = candidates[i];
		//bool distant = isDistant(candidate);
		bool distant = gridOfAccepted.isDistant(candidate);

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

void processChunk(Chunk* chunk) {

	Points* points = chunk->points;
	cout << "count: " << points->points.size() << endl;
	cout << chunk->min.toString() << endl;
	cout << chunk->max.toString() << endl;

	double tStart = now();

	Vector3<double> chunkSize = chunk->max - chunk->min;

	int numPoints = points->points.size();
	double level = std::floor(std::log(double(numPoints)) / std::log(4.0));
	double estimatedSpacing = std::pow(2.0, level + 1.0);

	//double spacing = chunkSize.x / 2048;
	double spacing = 0.3;

	int gridSize = 0;

	if (numPoints > 10'000'000) {
		gridSize = 1;
	}else if (numPoints > 4'000'000) {
		gridSize = 5;
	} else if (numPoints > 1'000'000) {
		gridSize = 4;
	} else if (numPoints > 400'000) {
		gridSize = 3;
	} else if (numPoints > 100'000) {
		gridSize = 2;
	}
	vector<Points*> cells;

	gridSize = 100;
	
	if(numPoints < 100'000){
		cells = { points };
	} else {
		Vector3<double> min = chunk->min;
		Vector3<double> max = chunk->max;
		cells = split(points, gridSize, min, max);
	}


	printElapsedTime("cells", tStart);

	cout << "#cells" << cells.size() << endl;

	GridOfAccepted acceptedL0(512, chunk->min, chunk->max, spacing);
	GridOfAccepted acceptedL1(256, chunk->min, chunk->max, 2.0 * spacing);
	GridOfAccepted acceptedL2(128, chunk->min, chunk->max, 4.0 * spacing);
	
	vector<Point> accepted;
	for (Points* cell : cells) {

		std::sort(cell->points.begin(), cell->points.end(), compare);

		vector<Point> sampleAccepted = subsample(cell->points, spacing, acceptedL0);
		accepted.insert(accepted.end(), sampleAccepted.begin(), sampleAccepted.end());


		//std::sort(accepted.begin(), accepted.end(), compare);
	}

	auto level1 = subsample(accepted, spacing * 2.0, acceptedL1);
	auto level2 = subsample(level1, spacing * 4.0, acceptedL2);

	//vector<Point> level1;
	//vector<Point> level2;
	//subsample(accepted, spacing * 2.0, level1);
	//subsample(level1, spacing * 4.0, level2);
	//auto level3 = subsample(level2, spacing * 8.0, gridOfAcceptedL3);
	//auto level4 = subsample(level3, spacing * 16.0, gridOfAcceptedL4);
	//auto level5 = subsample(&level4[0], level4.size(), spacing * 32.0);
	//

	printElapsedTime("processing", tStart);

	cout << "estimated spacing? " << estimatedSpacing << endl;
	cout << "#accepted: " << accepted.size() << endl;
	cout << "#level1: " << level1.size() << endl;
	//cout << "#level2: " << level2.size() << endl;


	auto writeToDisk = [](vector<Point> points, string path) {

		fstream file;
		file.open(path, std::ios::out);

		file << std::fixed << std::setprecision(4);

		for (auto& point : points) {
			file << point.x << ", " << point.y << ", " << point.z << endl;
		}

		file.close();

	};

	cout << "checks l0: " << acceptedL0.maxChecks << endl;
	cout << "checks l1: " << acceptedL1.maxChecks << endl;
	cout << "checks l2: " << acceptedL2.maxChecks << endl;

	writeToDisk(accepted, "C:/temp/level0.csv");
	writeToDisk(level1, "C:/temp/level1.csv");
	writeToDisk(level2, "C:/temp/level2.csv");
	//writeToDisk(level3, "C:/temp/level3.csv");
	//writeToDisk(level4, "C:/temp/level4.csv");
	//writeToDisk(level5, "C:/temp/level5.csv");


}