
#include <unordered_map>
#include <fstream>
#include <iterator>

#include "ChunkProcessor.h"

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

bool compare(Point& a, Point& b) {

	auto da = (a.x + a.y + a.z) / SQRT_2;
	auto db = (b.x + b.y + b.z) / SQRT_2;

	return da < db;
}

double squaredDistance(Point& a, Point& b) {
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	double dz = b.z - a.z;

	double dd = dx * dx + dy * dy + dz * dz;

	return dd;
}


vector<Point> subsample(vector<Point> candidates, double spacing) {

	double squaredSpacing = spacing * spacing;
	int maxCount = 0;

	vector<Point> accepted;

	auto isDistant = [&maxCount, &accepted, spacing, squaredSpacing](Point& candidate) -> bool {

		int count = 0;
		for (int i = accepted.size() - 1; i >= 0; i--) {

			Point previouslyAccepted = accepted[i];

			auto d1 = (previouslyAccepted.x + previouslyAccepted.y + previouslyAccepted.z) / SQRT_2;
			auto d2 = (candidate.x + candidate.y + candidate.z) / SQRT_2;

			auto d = d2 - d1;

			if (std::abs(d) > spacing) {
				maxCount = std::max(maxCount, count);
				return true;
			}


			double dd = squaredDistance(candidate, previouslyAccepted);

			if (dd < squaredSpacing) {
				maxCount = std::max(maxCount, count);
				return false;
			}

			count++;
			//if (count > 1000) {
			//	return true;
			//}
		}

		maxCount = std::max(maxCount, count);

		return true;

	};

	int numPoints = candidates.size();
	for (int i = 0; i < numPoints; i++) {

		Point candidate = candidates[i];
		bool distant = isDistant(candidate);

		if (distant) {
			accepted.push_back(candidate);
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
	Vector3<double> cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);
	vector<Cell*> grid(gridSize * gridSize * gridSize, nullptr);
	vector<Cell*> cells;

	for (int i = 0; i < input->points.size(); i++) {

		Point point = input->points[i];

		int32_t ux = int32_t(cellsD.x * (point.x - min.x) / size.x);
		int32_t uy = int32_t(cellsD.y * (point.y - min.y) / size.y);
		int32_t uz = int32_t(cellsD.z * (point.z - min.z) / size.z);

		ux = std::min(ux, gridSize - 1);
		uy = std::min(uy, gridSize - 1);
		uz = std::min(uz, gridSize - 1);

		int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

		Cell* cell = grid[index];

		if (cell == nullptr) {
			cell = new Cell();
			cell->points = new Points();
			cell->ux = ux;
			cell->uy = uy;
			cell->uz = uz;
			grid[index] = cell;

			cells.push_back(cell);
		}

		cell->points->points.push_back(point);
	}

	std::sort(cells.begin(), cells.end(), [](Cell* a, Cell* b) {

		int da = a->ux + a->uy + a->uz;
		int db = b->ux + b->uy + b->uz;

		return da < db;
	});

	vector<Points*> pointCells;
	for (Cell* cell : cells) {
		pointCells.push_back(cell->points);
	}

	return pointCells;
}

void processChunk(Chunk* chunk) {

	Points* points = chunk->points;
	cout << "count: " << points->points.size() << endl;
	cout << chunk->min.toString() << endl;
	cout << chunk->max.toString() << endl;

	double tStart = now();

	std::sort(points->points.begin(), points->points.end(), compare);

	Vector3<double> chunkSize = chunk->max - chunk->min;

	int numPoints = points->points.size();
	double level = std::floor(std::log(double(numPoints)) / std::log(4.0));
	double estimatedSpacing = std::pow(2.0, level + 1.0);

	double spacing = chunkSize.x / 1024;

	int gridSize = 0;

	if (numPoints > 10'000'000) {
		gridSize = 6;
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
	
	if(numPoints < 100'000){
		cells = { points };
	} else {
		Vector3<double> min = chunk->min;
		Vector3<double> max = chunk->max;
		cells = split(points, gridSize, min, max);
	}


	printElapsedTime("cells", tStart);

	cout << "#cells" << cells.size() << endl;
	
	vector<Point> accepted;
	for (Points* cell : cells) {
		vector<Point> sampleAccepted = subsample(cell->points, spacing);

		std::sort(sampleAccepted.begin(), sampleAccepted.end(), compare);

		accepted.insert(accepted.end(), sampleAccepted.begin(), sampleAccepted.end());
	}
	//std::sort(accepted.begin(), accepted.end(), compare);
	
	//vector<Point> accepted = subsample(points->points, spacing);

	auto level1 = subsample(accepted, spacing * 2.0);
	auto level2 = subsample(level1, spacing * 4.0);
	//auto level3 = subsample(&level2[0], level2.size(), spacing * 8.0);
	//auto level4 = subsample(&level3[0], level3.size(), spacing * 16.0);
	//auto level5 = subsample(&level4[0], level4.size(), spacing * 32.0);
	//

	printElapsedTime("processing", tStart);

	cout << "estimated spacing? " << estimatedSpacing << endl;
	cout << "#accepted: " << accepted.size() << endl;
	cout << "#level1: " << level1.size() << endl;
	cout << "#level2: " << level2.size() << endl;


	auto writeToDisk = [](vector<Point> points, string path) {

		fstream file;
		file.open(path, std::ios::out);

		for (auto& point : points) {
			file << point.x << ", " << point.y << ", " << point.z << endl;
		}

		file.close();

	};

	writeToDisk(accepted, "C:/temp/level0.csv");
	writeToDisk(level1, "C:/temp/level1.csv");
	writeToDisk(level2, "C:/temp/level2.csv");
	//writeToDisk(level3, "C:/temp/level3.csv");
	//writeToDisk(level4, "C:/temp/level4.csv");
	//writeToDisk(level5, "C:/temp/level5.csv");


}