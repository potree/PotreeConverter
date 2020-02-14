
#include "Indexer_Centered_countsort.h"

#include <vector>
#include <filesystem>
#include <memory>
#include <thread>
#include <unordered_map>

#include "json.hpp"

#include "convmath.h"
#include "converter_utils.h"
#include "stuff.h"
#include "LASWriter.hpp"
#include <bitset>


using std::vector;
using std::thread;
using std::unordered_map;
using std::string;
using std::to_string;
using std::make_shared;
using std::shared_ptr;

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace countsort{



	struct Chunk {
		Vector3<double> min;
		Vector3<double> max;

		string file;
		string id;
	};

	vector<shared_ptr<Chunk>> getListOfChunks(string pathIn) {
		string chunkDirectory = pathIn + "/chunks";

		auto toID = [](string filename) -> string {
			string strID = stringReplace(filename, "chunk_", "");
			strID = stringReplace(strID, ".bin", "");

			return strID;
		};

		vector<shared_ptr<Chunk>> chunksToLoad;
		for (const auto& entry : fs::directory_iterator(chunkDirectory)) {
			string filename = entry.path().filename().string();
			string chunkID = toID(filename);

			if (!iEndsWith(filename, ".bin")) {
				continue;
			}

			shared_ptr<Chunk> chunk = make_shared<Chunk>();
			chunk->file = entry.path().string();
			chunk->id = chunkID;

			string metadataText = readTextFile(chunkDirectory + "/metadata.json");
			json js = json::parse(metadataText);

			/*Vector3<double> min = metadata.min;
			Vector3<double> max = metadata.max;*/

			Vector3<double> min = {
				js["min"][0].get<double>(),
				js["min"][1].get<double>(),
				js["min"][2].get<double>()
			};
			Vector3<double> max = {
				js["max"][0].get<double>(),
				js["max"][1].get<double>(),
				js["max"][2].get<double>()
			};

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
	
	//struct Bin {
	//	uint64_t index;
	//	uint64_t start;
	//	uint64_t size;
	//};

	//vector<Bin> binsort(vector<Point>& points, int gridSize, 
	//	Vector3<double> min, Vector3<double> max) {


	//	double dGridSize = gridSize;
	//	auto size = max - min;
	//	vector<int> counts(gridSize * gridSize * gridSize, 0);
	//	vector<Bin> bins;

	//	auto toIndex = [dGridSize, min, size, gridSize](Point& point){
	//		int64_t ix = std::min(dGridSize * (point.x - min.x) / size.x, dGridSize - 1.0);
	//		int64_t iy = std::min(dGridSize * (point.y - min.y) / size.y, dGridSize - 1.0);
	//		int64_t iz = std::min(dGridSize * (point.z - min.z) / size.z, dGridSize - 1.0);

	//		int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

	//		return index;
	//	};

	//	//================
	//	//== COUNT
	//	//================
	//	for (Point point : points) {

	//		// int64_t ix = std::min(dGridSize * (point.x - min.x) / size.x, dGridSize - 1.0);
	//		// int64_t iy = std::min(dGridSize * (point.y - min.y) / size.y, dGridSize - 1.0);
	//		// int64_t iz = std::min(dGridSize * (point.z - min.z) / size.z, dGridSize - 1.0);

	//		// int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

	//		int64_t index = toIndex(point);

	//		int count = counts[index]++;

	//		if(count == 0){
	//			Bin bin = {index, 0, 0};
	//			bins.push_back(bin);
	//		}
	//	}

	//	int sum = 0;
	//	for(Bin& bin : bins){
	//		bin.start = sum;
	//		bin.size = counts[bin.index];

	//		sum += bin.size;
	//	}


	//	//================
	//	//== SORT
	//	//================




	//	return bins;
	//}


	// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
	// method to seperate bits from a given integer 3 positions apart
	inline uint64_t splitBy3(uint32_t a){
		uint64_t x = a & 0x1fffff; // we only look at the first 21 bits
		x = (x | x << 32) & 0x1f00000000ffff; // shift left 32 bits, OR with self, and 00011111000000000000000000000000000000001111111111111111
		x = (x | x << 16) & 0x1f0000ff0000ff; // shift left 32 bits, OR with self, and 00011111000000000000000011111111000000000000000011111111
		x = (x | x << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, and 0001000000001111000000001111000000001111000000001111000000000000
		x = (x | x << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, and 0001000011000011000011000011000011000011000011000011000100000000
		x = (x | x << 2) & 0x1249249249249249;
		return x;
	}

	// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
	inline uint64_t mortonEncode_magicbits(uint32_t x, uint32_t y, uint32_t z){
		uint64_t answer = 0;
		answer |= splitBy3(x) | splitBy3(y) << 1 | splitBy3(z) << 2;
		return answer;
	}

	double sum = 0;

	// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
	void mortonSort(vector<Point> points, Vector3<double> min, Vector3<double> max, int levels){

		auto size = max - min;
		int gridSize = pow(2, levels);
		double dGridSize = gridSize;

		for(Point& point : points){
			
			uint32_t x = std::min((point.x - min.x) * (dGridSize / size.x), dGridSize - 1.0);
			uint32_t y = std::min((point.y - min.y) * (dGridSize / size.y), dGridSize - 1.0);
			uint32_t z = std::min((point.z - min.z) * (dGridSize / size.z), dGridSize - 1.0);
			
			uint64_t mortonCode = mortonEncode_magicbits(x, y, z);
			point.index = mortonCode;

		}

		std::sort(points.begin(), points.end(), [](Point& a, Point& b){
			return a.index - b.index;
		});



		//for (int i = 0; i < points.size(); i++) {



		//}

		//int i = 0;
		//int levels = 8;
		//uint32_t currCellCode = (points[0].index >> (3 * levels));
		//Point

		//for (Point& point : points) {
		//	sum += point.x * point.y * point.z;

		//	uint32_t mortonCode = point.index;
		//	uint32_t cellCode = (mortonCode >> (3 * level)) & 0b111;

		//	if (cellCode != currCellCode) {


		//		currCelCode = cellCode;
		//	}

		//	std::bitset<32> bcode(mortonCode);

		//	if (i < 100) {
		//		cout << bcode << endl;
		//	}

		//	i++;
		//}

	}



	void doIndexing(string path) {

		fs::create_directories(path + "/nodes");

		auto chunks = getListOfChunks(path);

		auto chunk = chunks[0];
		auto points = loadPoints(chunk->file);

		auto tStart = now();
		
		//vector<Bin> bins = binsort(points, 128, chunk->min, chunk->max);

		//std::sort(points.begin(), points.end(), [](Point& a, Point& b) -> bool {
		//	return a.x - b.x;
		//});

		//int levels = 5;
		//int gridSize = 128 * pow(2, levels);
		int gridSize = 512;
		int levels = 9;
		//int gridSize = pow(2, levels);
		mortonSort(points, chunk->min, chunk->max, levels);

		
		printElapsedTime("binsort", tStart);
		cout << "#points: " << points.size() << endl;
		//cout << "#bins: " << bins.size() << endl;
		cout << "sum: " << sum << endl;


	}

}