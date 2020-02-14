
#include "Indexer_Bluenoise.h"

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
#include "LASLoader.hpp"
#include "TaskPool.h"


using std::vector;
using std::thread;
using std::unordered_map;
using std::string;
using std::to_string;
using std::make_shared;
using std::shared_ptr;
using json = nlohmann::json;

namespace fs = std::filesystem;

namespace bluenoise {

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


//void indexChunk(shared_ptr<Chunk> chunk, string path) {
//
//	auto points = loadPoints(chunk->file);
//
//	cout << "test" << endl;
//	cout << "size: " << points.size() << endl;
//
//	auto tStart = now();
//
//	int gridSize = 32;
//	double dGridSize = gridSize;
//	auto min = chunk->min;
//	auto max = chunk->max;
//	auto size = max - min;
//
//	//auto indexOf = [min, max, size, dGridSize, gridSize](Point& point) -> int64_t {
//
//	//	double dx = dGridSize * (point.x - min.x) / size.x;
//	//	double dy = dGridSize * (point.y - min.y) / size.y;
//	//	double dz = dGridSize * (point.z - min.z) / size.z;
//
//	//	int64_t ix = std::min(dx, dGridSize - 1.0);
//	//	int64_t iy = std::min(dy, dGridSize - 1.0);
//	//	int64_t iz = std::min(dz, dGridSize - 1.0);
//
//	//	int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;
//
//	//	return index;
//	//};
//
//	struct Index {
//		int x;
//		int y;
//		int z;
//		int w;
//	};
//
//	auto indexOf = [min, max, size, dGridSize, gridSize](Point& point) -> Index {
//
//		double dx = dGridSize * (point.x - min.x) / size.x;
//		double dy = dGridSize * (point.y - min.y) / size.y;
//		double dz = dGridSize * (point.z - min.z) / size.z;
//
//		int ix = std::min(dx, dGridSize - 1.0);
//		int iy = std::min(dy, dGridSize - 1.0);
//		int iz = std::min(dz, dGridSize - 1.0);
//		
//		int index = ix + iy * gridSize + iz * gridSize * gridSize;
//
//		return { ix, iy, iz, index };
//	};
//
//
//
//	struct Bin {
//		Index index;
//		vector<Point> points;
//	};
//
//	vector<Bin> bins(gridSize * gridSize * gridSize);
//	vector<Bin> populatedBins;
//	for (Point& point : points) {
//		auto index = indexOf(point);
//
//		Bin& bin = bins[index.w];
//
//		if (bin.points.size() == 0) {
//			bin.index = index;
//			populatedBins.push_back(bin);
//		}
//
//		bin.points.push_back(point);
//	}
//
//
//	//vector<int> counts(gridSize * gridSize * gridSize, 0);
//	//for (Point& point : points) {
//	//	auto index = indexOf(point);
//
//	//	counts[index]++;
//	//}
//
//	//int numBins = 0;
//	//for(int i = 0; i < counts.size(); i++){
//	//	int count = counts[i];
//
//	//	if(count > 0){
//	//		numBins++;
//	//	}
//	//}
//
//	//std::sort(points.begin(), points.end(), [](Point& a, Point& b) {
//	//	return a.x - b.x;
//	//});
//
//	double sum = 0;
//	//for (Point& point : points) {
//	//	sum += point.x;
//	//}
//
//	printElapsedTime("indexing(raw)", tStart);
//	cout << "#bins: " << populatedBins.size() << endl;
//	//cout << "#bins: " << numBins << endl;
//	cout << "sum: " << sum << endl;
//
//
//}












struct Node {

	Vector3<double> min;
	Vector3<double> max;
	string name;

	vector<Point> points;
	vector<Point> store;
	int64_t numPoints = 0;

	int storeSize = 20'000;
	bool storeExceeded = false;

	Node* parent = nullptr;
	Node* children[8] = { 
		nullptr, nullptr, nullptr, nullptr, 
		nullptr, nullptr, nullptr, nullptr};

	Node(string name, Vector3<double> min, Vector3<double> max) {
		this->name = name;
		this->min = min;
		this->max = max;

		store.reserve(10'000);
	}

	void processStore() {
		storeExceeded = true;

		for (Point& point : store) {
			add(point);
		}

		store.clear();
		store.shrink_to_fit();
	}

	void add(Point& point) {

		numPoints++;

		if (!storeExceeded) {
			store.push_back(point);

			if (store.size() == storeSize) {
				processStore();
			}

			return;
		}

		
		int childIndex = childIndexOf(min, max, point);

		Node* child = children[childIndex];

		if (child == nullptr) {

			string childName = name + to_string(childIndex);
			auto box = childBoundingBoxOf(min, max, childIndex);

			child = new Node(childName, box.min, box.max);
			child->parent = this;
			children[childIndex] = child;
		}

		child->add(point);
	}

	void traverse(function<void(Node*)> callback) {

		callback(this);

		for (auto child : children) {
			if (child == nullptr) {
				continue;
			}

			child->traverse(callback);
		}

	}

	void traverse_postorder(function<void(Node*)> callback){
		for (auto child : children) {
			if (child == nullptr) {
				continue;
			}

			child->traverse_postorder(callback);
		}

		callback(this);
	}

};


int maxChecks = 0;

void indexChunk(shared_ptr<Chunk> chunk, string path) {

	auto points = loadPoints(chunk->file);

	cout << "test" << endl;
	cout << "size: " << points.size() << endl;

	auto tStart = now();

	int gridSize = 32;
	double dGridSize = gridSize;
	auto min = chunk->min;
	auto max = chunk->max;
	auto size = max - min;


	Node root(chunk->id, min, max);

	for (Point& point : points) {
		root.add(point);
	}

	printElapsedTime("indexing", tStart);

	auto tSort = now();

	root.traverse_postorder([](Node* node){

		//cout << "visiting " << node->name << endl;

		int level = node->name.size() - 1;

		auto size = node->max - node->min;
		auto center = node->min + size * 0.5;

		std::sort(node->store.begin(), node->store.end(), [center](Point& a, Point& b) {

			//return a.x - b.x;
			double da = a.squaredDistanceTo(center);
			double db = b.squaredDistanceTo(center);

			return da > db;
		});

		double baseSpacing = 0.2;
		double spacing = baseSpacing / pow(2.0, level);
		double spacingSquared = spacing * spacing;
		vector<Point> accepted;
		vector<Point> rejected;

		auto isDistant = [&accepted, center, spacingSquared, spacing](Point& candidate){

			int checks = 0;

			for(Point& prev : accepted){

				//double dc = sqrt(candidate.squaredDistanceTo(center));
				//double pc = sqrt(prev.squaredDistanceTo(center));

				//if (pc > dc + spacing) {
				//	return true;
				//}


				checks++;
				maxChecks = std::max(maxChecks, checks);

				auto distanceSquared = candidate.squaredDistanceTo(prev);

				if(distanceSquared < spacingSquared){
					return false;
				}
			}

			return true;
		};

		for(Point& candidate : node->store){
			bool distant = isDistant(candidate);

			if(distant){
				accepted.push_back(candidate);
			}else{
				rejected.push_back(candidate);
			}
		}

		node->points = accepted;
		node->store = rejected;

		if(node->parent != nullptr){
			Node* parent = node->parent;

			//cout << "adding " << accepted.size() << " points from " << node->name << " to " << parent->name << endl;
			parent->store.insert(parent->store.end(), accepted.begin(), accepted.end());
		}

		//cout << repeat("  ", level) << node->name << ": " << accepted.size() << endl;

	});

	printElapsedTime("sorting", tSort);


	root.traverse([](Node* node) {

		string path = "D:/temp/test/nodes/" + node->name + ".las";

		vector<Point> points;
		points.reserve(node->points.size() + node->store.size());

		points.insert(points.begin(), node->points.begin(), node->points.end());
		points.insert(points.end(), node->store.begin(), node->store.end());

		LASHeader header;
		header.min = node->min;
		header.max = node->max;
		header.numPoints = points.size();

		writeLAS(path, header, points);

	});

	cout << "maxChecks: " << maxChecks << endl;
}



void doIndexing(string path) {

	//fs::create_directories(path + "/nodes");

	auto chunks = getListOfChunks(path);

	auto chunk = chunks[5];
	indexChunk(chunk, path);


	//TaskPool<IndexTask> pool(16, processor);

	//for(auto chunk : chunks){
	//	shared_ptr<IndexTask> task = make_shared<IndexTask>(chunk, path);

	//	pool.addTask(task);
	//}

	//pool.close();


	//
	//cout << "pointsProcessed: " << pointsProcessed << endl;


}
}