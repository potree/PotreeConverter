
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


void processChunk(Chunk* chunk) {

	cout << "count: " << chunk->points->count << endl;

	cout << chunk->min.toString() << endl;
	cout << chunk->max.toString() << endl;

	Points* points = chunk->points;
	Attribute& aPosition = points->attributes[0];
	double* dPosition = points->data[0]->dataD;
	Vector3<double>* position = reinterpret_cast<Vector3<double>*>(dPosition);

	std::sort(position, position + points->count, compare);

	vector<Vector3<double>> accepted;
	double spacing = 0.01;
	double squaredSpacing = spacing * spacing;
	int maxCount = 0;

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

	for (int i = 0; i < points->count; i++) {

		Vector3<double> candidate = position[i];
		bool distant = isDistant(candidate);

		if (distant) {
			accepted.push_back(candidate);
		}

	}

	cout << "maxCount: " << maxCount << endl;
	cout << "#accepted: " << accepted.size() << endl;


	{

		string path = "C:/temp/subsample.csv";
		fstream file;
		file.open(path, std::ios::out);

		for (auto& point : accepted) {
			file << point.x << ", " << point.y << ", " << point.z << endl;
		}

		file.close();

	}


}