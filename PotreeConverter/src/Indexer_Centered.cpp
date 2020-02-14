
#include "Indexer_Centered.h"

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

namespace centered{

struct Chunk {
	Vector3<double> min;
	Vector3<double> max;

	string file;
	string id;
};

int pointsAdded = 0;

struct Node {

	string name;
	int level;
	int gridSize;
	double dGridSize;

	vector<Point> store;
	int storeSize = 1'000;
	bool storeBroken = false;

	int maxDepth = 0;

	unordered_map<int, Point> grid;

	BoundingBox box;
	vector<shared_ptr<Node>> children;

	Node(string name, int gridSize, BoundingBox box, int maxDepth) {
		this->name = name;
		this->level = name.size() - 1;
		this->gridSize = gridSize;
		this->box = box;
		this->maxDepth = maxDepth;

		children.resize(8, nullptr);
		//store.reserve(storeSize);

		dGridSize = gridSize;
	}

	void breakStore() {
		storeBroken = true;

		for (Point& point : store) {
			this->add(point);
		}

		store.clear();
		store = vector<Point>();
	}

	void add(Point& point) {

		//if (!storeBroken) {
		//	store.push_back(point);

		//	if (store.size() > storeSize) {
		//		breakStore();
		//	}

		//	return;
		//}

		if (level >= maxDepth) {
			store.push_back(point);
			return;
		}

		auto min = this->box.min;
		auto max = this->box.max;
		auto size = max - min;

		int64_t ix = std::min(dGridSize * (point.x - min.x) / size.x, dGridSize - 1.0);
		int64_t iy = std::min(dGridSize * (point.y - min.y) / size.y, dGridSize - 1.0);
		int64_t iz = std::min(dGridSize * (point.z - min.z) / size.z, dGridSize - 1.0);

		int64_t index = ix + gridSize * iy + gridSize * gridSize * iz;

		
		//Point prev = grid[index];

		if (grid.find(index) == grid.end()) {
			//grid[index] = point;
			grid.insert(std::make_pair(index, point));

			pointsAdded++;
		} else {

			Point prev = grid[index];

			double p1x = (dGridSize * (point.x - min.x) / size.x) - double(ix) - 0.5;
			double p1y = (dGridSize * (point.y - min.y) / size.y) - double(iy) - 0.5;
			double p1z = (dGridSize * (point.z - min.z) / size.z) - double(iz) - 0.5;

			double p2x = (dGridSize * (prev.x - min.x) / size.x) - double(ix) - 0.5;
			double p2y = (dGridSize * (prev.y - min.y) / size.y) - double(iy) - 0.5;
			double p2z = (dGridSize * (prev.z - min.z) / size.z) - double(iz) - 0.5;

			double d1 = p1x * p1x + p1y * p1y + p1z * p1z;
			double d2 = p2x * p2x + p2y * p2y + p2z * p2z;

			// d1 is closer to center
			// 1: swap
			// 2: move previous point down to child
			if (d1 < d2) {
				grid[index] = point;

				int childIndex = computeChildIndex(box, prev);

				if (children[childIndex] == nullptr) {

					string childName = name + to_string(childIndex);
					auto childBox = childBoundingBoxOf(box, childIndex);

					auto child = make_shared<Node>(childName, gridSize, childBox, maxDepth);
					children[childIndex] = child;
				}

				children[childIndex]->add(prev);
			} else {
				int childIndex = computeChildIndex(box, point);

				if (children[childIndex] == nullptr) {

					string childName = name + to_string(childIndex);
					auto childBox = childBoundingBoxOf(box, childIndex);

					auto child = make_shared<Node>(childName, gridSize, childBox, maxDepth);
					children[childIndex] = child;
				}

				children[childIndex]->add(point);
			}

			

		}


	}

	vector<Point> getAccepted() {

		vector<Point> accepted;

		//for (Point point : grid) {
		for (auto it : grid){
			Point point = it.second;

			if (point.index == 0) {
				continue;
			}

			accepted.push_back(point);
		}

		return accepted;

	}

	void traverse(std::function<void(Node*)> callback) {

		callback(this);

		for (auto child : children) {
			if (child == nullptr) {
				continue;
			}

			child->traverse(callback);
		}

	}

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

int pointsProcessed = 0;

shared_ptr<Node> indexChunk(shared_ptr<Chunk> chunk, string path) {
	auto points = loadPoints(chunk->file);

	auto tStartIndexing = now();

	int gridSize = 128;

	BoundingBox box = { chunk->min, chunk->max };
	shared_ptr<Node> root = make_shared<Node>(chunk->id, gridSize, box, 5);

	for (Point point : points) {
		root->add(point);
	}

	int numNodes = 0;
	int highestLevel = 0;
	root->traverse([&numNodes, &highestLevel](Node* node) {
		//cout << repeat("  ", node->level) << node->name << endl;
		numNodes++;
		highestLevel = std::max(highestLevel, node->level);
	});

	//cout << "numNodes: " << numNodes << endl;
	//cout << "highestLevel: " << highestLevel << endl;

	//auto accepted = root.getAccepted();
	//auto accepted = root.children[0]->getAccepted();

	printElapsedTime("indexing " + chunk->id, tStartIndexing);

	pointsProcessed += points.size();

	return root;

	//cout << "pointsAdded: " << pointsAdded << endl;

	// vector<Point> resolved;
	// resolved.reserve(points.size());


	//root.traverse([path](Node* node) {

	//	auto accepted = node->getAccepted();

	//	if (accepted.size() > 0) {
	//		LASHeader header;
	//		header.numPoints = accepted.size();
	//		header.headerSize = 375;
	//		header.scale = { 0.001, 0.001, 0.001 };
	//		header.min = node->box.min;
	//		header.max = node->box.max;
	//		string laspath = path + "/nodes/" + node->name + ".las";
	//		writeLAS(laspath, header, accepted);
	//	}

	//	if (node->store.size() > 0) {
	//		LASHeader header;
	//		header.numPoints = node->store.size();
	//		header.headerSize = 375;
	//		header.scale = { 0.001, 0.001, 0.001 };
	//		header.min = node->box.min;
	//		header.max = node->box.max;
	//		string laspath = path + "/nodes/" + node->name + "_store.las";
	//		writeLAS(laspath, header, node->store);
	//	}

	//	

	//});




	//LASHeader header;
	//header.numPoints = resolved.size();
	//header.headerSize = 375;
	//header.scale = { 0.001, 0.001, 0.001 };
	//header.min = chunk->min;
	//header.max = chunk->max;
	//string laspath = path + "/nodes/" + chunk->id + ".las";
	//writeLAS(laspath, header, resolved);
}

struct IndexTask {
	shared_ptr<Chunk> chunk;
	string path;

	IndexTask(shared_ptr<Chunk> chunk, string path) {
		this->chunk = chunk;
		this->path = path;
	}
};

auto processor = [](shared_ptr<IndexTask> task) {
	auto root = indexChunk(task->chunk, task->path);


};

void doIndexing(string path) {

	fs::create_directories(path + "/nodes");

	auto chunks = getListOfChunks(path);

	// auto chunk = chunks[0];
	// indexChunk(chunk, path);


	TaskPool<IndexTask> pool(16, processor);

	for(auto chunk : chunks){
		shared_ptr<IndexTask> task = make_shared<IndexTask>(chunk, path);

		pool.addTask(task);
	}

	pool.close();


	
	cout << "pointsProcessed: " << pointsProcessed << endl;


}
}