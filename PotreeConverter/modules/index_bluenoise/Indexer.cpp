
#include "Indexer.h"

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
#include "Structures.h"
#include "PotreeWriter.h"


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


struct Indexer{

	shared_ptr<Node> root;
	shared_ptr<Chunks> chunks;

	Indexer(){

	}

};

// create hierarchy from root "r" to the nodes represented by all chunks
shared_ptr<Node> buildHierarchyToRoot(shared_ptr<Chunks> chunks){

	auto expandHierarchy = [](shared_ptr<Node> root, string name){

		shared_ptr<Node> node = root;
		for(int i = 1; i < name.size(); i++){
			int childIndex = name.at(i) - '0';

			auto child = node->children[childIndex];

			if(child == nullptr){
				string childName = name.substr(0, i + 1);
				auto box = childBoundingBoxOf(node->min, node->max, childIndex);

				child = make_shared<Node>(childName, box.min, box.max);
				child->parent = node.get();
				node->children[childIndex] = child;
			}

			node = child;
		}

	};

	shared_ptr<Node> root = make_shared<Node>("r", chunks->min, chunks->max);

	for(auto chunk : chunks->list){
		expandHierarchy(root, chunk->id);
	}

	return root;
}


shared_ptr<Chunks> getChunks(string pathIn) {
	string chunkDirectory = pathIn + "/chunks";

	string metadataText = readTextFile(chunkDirectory + "/metadata.json");
	json js = json::parse(metadataText);

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

		BoundingBox box = { min, max };

		for (int i = 1; i < chunkID.size(); i++) {
			int index = chunkID[i] - '0'; // this feels so wrong...

			box = childBoundingBoxOf(box, index);
		}

		chunk->min = box.min;
		chunk->max = box.max;

		chunksToLoad.push_back(chunk);
	}

	auto chunks = make_shared<Chunks>(chunksToLoad, min, max);

	return chunks;
}












int maxChecks = 0;

void indexNode(shared_ptr<Node> chunkRoot, double baseSpacing){

	function<void(shared_ptr<Node>, function<void(Node*)>)> traverse;
	traverse = [&traverse](shared_ptr<Node> node, function<void(Node*)> callback) {
		for (auto child : node->children) {
			if (child == nullptr || child->isFlushed) {
				continue;
			}

			traverse(child, callback);
		}

		callback(node.get());
	};


	traverse(chunkRoot, [baseSpacing](Node* node){

		int level = node->name.size() - 1;
		auto size = node->max - node->min;
		auto center = node->min + size * 0.5;

		double spacing = baseSpacing / pow(2.0, level);
		double spacingSquared = spacing * spacing;
		vector<Point> accepted;
		vector<Point> rejected;

		std::sort(node->store.begin(), node->store.end(), [center](Point& a, Point& b) {
			//return a.x - b.x;
			double da = a.squaredDistanceTo(center);
			double db = b.squaredDistanceTo(center);

			return da < db;
		});

		auto isDistant = [&accepted, center, spacingSquared, spacing](Point& candidate){

			int checks = 0;

			for(int i = accepted.size() - 1; i >= 0; i--){
				Point& prev = accepted[i];

				double dc = sqrt(candidate.squaredDistanceTo(center));
				double pc = sqrt(prev.squaredDistanceTo(center));

				if (dc > pc + spacing) {
					return true;
				}

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

}

shared_ptr<IndexedChunk> indexChunk(shared_ptr<Chunk> chunk, shared_ptr<Node> chunkRoot, string path, double baseSpacing) {

	auto points = loadPoints(chunk->file);

	cout << "test" << endl;
	cout << "size: " << points.size() << endl;

	auto tStart = now();

	int gridSize = 32;
	double dGridSize = gridSize;
	auto min = chunk->min;
	auto max = chunk->max;
	auto size = max - min;

	for (Point& point : points) {
		chunkRoot->add(point);
	}

	printElapsedTime("indexing", tStart);

	auto tSort = now();

	int totalSortDuration = 0.0;

	auto tSubsampling = now();

	indexNode(chunkRoot, baseSpacing);

	printElapsedTime("subsampling", tSubsampling);
	cout << "sorting: " << totalSortDuration << "s" << endl;


	cout << "maxChecks: " << maxChecks << endl;

	auto index = make_shared<IndexedChunk>();
	index->chunk = chunk;
	index->node = chunkRoot;

	return index;
}



void doIndexing(string path) {

	// create base directory or remove files from previous build
	fs::create_directories(path + "/nodes");
	for (const auto& entry : std::filesystem::directory_iterator(path + "/nodes")) {
		std::filesystem::remove(entry);
	}
	std::filesystem::remove(path + "/octree.bin");



	auto chunks = getChunks(path);

	Indexer indexer;
	indexer.chunks = chunks;
	indexer.root = buildHierarchyToRoot(chunks);

	double spacing = chunks->max.distanceTo(chunks->min) / 100.0;

	PotreeWriter writer(path, chunks->min, chunks->max);

	//auto chunk = chunks->list[1];
	//indexChunk(chunk, path, spacing);

	struct IndexTask {
		shared_ptr<Chunk> chunk;
		shared_ptr<Node> node;
		string path;

		IndexTask(shared_ptr<Chunk> chunk, shared_ptr<Node> node, string path) {
			this->chunk = chunk;
			this->node = node;
			this->path = path;
		}
	};

	auto processor = [spacing, &writer](shared_ptr<IndexTask> task) {
		auto node = task->node;

		indexChunk(task->chunk, node, task->path, spacing);

		writer.writeChunk(node);

		node->clear();
	};

	TaskPool<IndexTask> pool(15, processor);

	for(auto chunk : chunks->list){
		auto node = Node::find(indexer.root, chunk->id);

		shared_ptr<IndexTask> task = make_shared<IndexTask>(chunk, node, path);

		pool.addTask(task);
	}

	pool.close();


	indexNode(indexer.root, spacing);
	writer.writeChunk(indexer.root);


	writer.close();




}
}