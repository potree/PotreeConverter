
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



shared_ptr<IndexedChunk> indexChunk(shared_ptr<Chunk> chunk, string path, double baseSpacing) {

	auto points = loadPoints(chunk->file);

	cout << "test" << endl;
	cout << "size: " << points.size() << endl;

	auto tStart = now();

	int gridSize = 32;
	double dGridSize = gridSize;
	auto min = chunk->min;
	auto max = chunk->max;
	auto size = max - min;


	shared_ptr<Node> root = make_shared<Node>(chunk->id, min, max);

	for (Point& point : points) {
		root->add(point);
	}

	printElapsedTime("indexing", tStart);

	auto tSort = now();

	//root.traverse_postorder([baseSpacing](Node* node) {

	//	auto size = node->max - node->min;
	//	auto center = node->min + size * 0.5;

	//	std::sort(node->store.begin(), node->store.end(), [center](Point& a, Point& b) {
	//		//return a.x - b.x;
	//		double da = a.squaredDistanceTo(center);
	//		double db = b.squaredDistanceTo(center);

	//		return da < db;
	//	});

	//});

	//printElapsedTime("sorting", tSort);

	int totalSortDuration = 0.0;

	auto tSubsampling = now();

	root->traverse_postorder([baseSpacing, &totalSortDuration](Node* node) {

		int level = node->name.size() - 1;
		auto size = node->max - node->min;
		auto center = node->min + size * 0.5;

		//double baseSpacing = 0.2;
		double spacing = baseSpacing / pow(2.0, level);
		double spacingSquared = spacing * spacing;
		vector<Point> accepted;
		vector<Point> rejected;

		double tSort = now();

		std::sort(node->store.begin(), node->store.end(), [center](Point& a, Point& b) {
			//return a.x - b.x;
			double da = a.squaredDistanceTo(center);
			double db = b.squaredDistanceTo(center);

			return da < db;
		});

		totalSortDuration += (now() - tSort);

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

	printElapsedTime("subsampling", tSubsampling);
	cout << "sorting: " << totalSortDuration << "s" << endl;


	//root.traverse([](Node* node) {

	//	string path = "D:/temp/test/nodes/" + node->name + ".las";

	//	vector<Point> points;
	//	points.reserve(node->points.size() + node->store.size());

	//	points.insert(points.begin(), node->points.begin(), node->points.end());
	//	points.insert(points.end(), node->store.begin(), node->store.end());

	//	LASHeader header;
	//	header.min = node->min;
	//	header.max = node->max;
	//	header.numPoints = points.size();

	//	writeLAS(path, header, points);

	//});

	cout << "maxChecks: " << maxChecks << endl;

	auto index = make_shared<IndexedChunk>();
	index->chunk = chunk;
	index->root = root;

	return index;
}



void doIndexing(string path) {

	fs::create_directories(path + "/nodes");

	for (const auto& entry : std::filesystem::directory_iterator(path + "/nodes")) {
		std::filesystem::remove(entry);
	}

	std::filesystem::remove(path + "/octree.bin");
	

	auto chunks = getChunks(path);

	double spacing = chunks->max.distanceTo(chunks->min) / 100.0;

	PotreeWriter writer(path, chunks->min, chunks->max);

	//auto chunk = chunks->list[1];
	//indexChunk(chunk, path, spacing);

	struct IndexTask {
		shared_ptr<Chunk> chunk;
		string path;

		IndexTask(shared_ptr<Chunk> chunk, string path) {
			this->chunk = chunk;
			this->path = path;
		}
	};

	auto processor = [spacing, &writer](shared_ptr<IndexTask> task) {
		auto index = indexChunk(task->chunk, task->path, spacing);

		writer.writeChunk(index);
	};

	TaskPool<IndexTask> pool(10, processor);

	for(auto chunk : chunks->list){
		shared_ptr<IndexTask> task = make_shared<IndexTask>(chunk, path);

		pool.addTask(task);
	}

	pool.close();
	writer.close();




}
}