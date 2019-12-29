
#include <unordered_map>

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

const double SQRT_2 = sqrt(2.0);

bool compare(Vector3<double>& a, Vector3<double>& b) {

	auto da = (a.x + a.y + a.z) / SQRT_2;
	auto db = (b.x + b.y + b.z) / SQRT_2;

	return da < db;
}


vector<Vector3<double>> subsample(Vector3<double>* candidates, int numPoints, double spacing) {

	double squaredSpacing = spacing * spacing;
	int maxCount = 0;

	vector<Vector3<double>> accepted;

	auto isDistant = [&maxCount, &accepted, spacing, squaredSpacing](Vector3<double>& candidate) -> bool {

		int count = 0;
		for (int i = accepted.size() - 1; i >= 0; i--) {

			Vector3<double>& previouslyAccepted = accepted[i];

			auto d1 = (previouslyAccepted.x + previouslyAccepted.y + previouslyAccepted.z) / SQRT_2;
			auto d2 = (candidate.x + candidate.y + candidate.z) / SQRT_2;

			auto d = d2 - d1;

			if (std::abs(d) > spacing) {
				maxCount = std::max(maxCount, count);
				return true;
			}


			auto squaredDistance = candidate.squaredDistanceTo(previouslyAccepted);

			if (squaredDistance < squaredSpacing) {
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

	for (int i = 0; i < numPoints; i++) {

		Vector3<double> candidate = candidates[i];
		bool distant = isDistant(candidate);

		if (distant) {
			accepted.push_back(candidate);
		}

	}

	return accepted;
}

void processChunk(Chunk* chunk) {

	cout << "count: " << chunk->points->count << endl;
	cout << chunk->min.toString() << endl;
	cout << chunk->max.toString() << endl;

	double tStart = now();

	Points* points = chunk->points;
	Attribute& aPosition = points->attributes[0];
	double* dPosition = points->data[0]->dataD;
	Vector3<double>* position = reinterpret_cast<Vector3<double>*>(dPosition);

	std::sort(position, position + points->count, compare);

	Vector3<double> chunkSize = chunk->max - chunk->min;

	double level = std::floor(std::log(double(points->count)) / std::log(4.0));
	double estimatedSpacing = std::pow(2.0, level + 1.0);

	double spacing = chunkSize.x / 1024;
	
	int numPoints = points->count;
	vector<Vector3<double>> accepted = subsample(position, numPoints, spacing);

	auto level1 = subsample(&accepted[0], accepted.size(), spacing * 2.0);
	auto level2 = subsample(&level1[0], level1.size(), spacing * 4.0);
	//auto level3 = subsample(&level2[0], level2.size(), spacing * 8.0);
	//auto level4 = subsample(&level3[0], level3.size(), spacing * 16.0);
	//auto level5 = subsample(&level4[0], level4.size(), spacing * 32.0);
	//

	printElapsedTime("processing", tStart);

	cout << "estimated spacing? " << estimatedSpacing << endl;
	cout << "#accepted: " << accepted.size() << endl;
	cout << "#level1: " << level1.size() << endl;
	cout << "#level2: " << level2.size() << endl;


	auto writeToDisk = [](vector<Vector3<double>> points, string path) {

		fstream file;
		file.open(path, std::ios::out);

		for (auto& point : points) {
			file << point.x << ", " << point.y << ", " << point.z << endl;
		}

		file.close();

	};

	//writeToDisk(accepted, "C:/temp/level0.csv");
	//writeToDisk(level1, "C:/temp/level1.csv");
	//writeToDisk(level2, "C:/temp/level2.csv");
	//writeToDisk(level3, "C:/temp/level3.csv");
	//writeToDisk(level4, "C:/temp/level4.csv");
	//writeToDisk(level5, "C:/temp/level5.csv");


}