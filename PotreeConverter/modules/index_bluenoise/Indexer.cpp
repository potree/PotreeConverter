
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
	shared_ptr<PotreeWriter> writer;
	double spacing = 1.0;

	Indexer(){

	}

	void indexNode(shared_ptr<Node> chunkRoot){

		function<void(shared_ptr<Node>, function<void(Node*)>)> traverse;
		traverse = [&traverse](shared_ptr<Node> node, function<void(Node*)> callback) {
			for (auto child : node->children) {
				if (child == nullptr || child->isSubsampled) {
					continue;
				}

				traverse(child, callback);
			}

			callback(node.get());
		};

		double baseSpacing = this->spacing;
		auto writer = this->writer;
		traverse(chunkRoot, [baseSpacing, writer](Node* node){

			int level = node->name.size() - 1;
			auto min = node->min;
			auto max = node->max;
			auto size = node->max - node->min;
			auto center = min + size * 0.5;

			double spacing = baseSpacing / pow(2.0, level);
			double spacingSquared = spacing * spacing;
			vector<Point> accepted;

			auto byCenter = [center](Point& a, Point& b) {
				//return a.x - b.x;
				double da = a.squaredDistanceTo(center);
				double db = b.squaredDistanceTo(center);

				return da > db;
			};

			//std::sort(node->store.begin(), node->store.end(), byCenter);

			auto isInConflictZone = [min, max, spacing](Point& candidate, Vector3<double>& center) {

				double wx = sin(3.1415 * candidate.x / spacing);
				double wy = sin(3.1415 * candidate.y / spacing);
				double wz = sin(3.1415 * candidate.z / spacing);

				double wxy = wx * wy;
				double wxz = wx * wz;
				double wyz = wy * wz;

				double adjust = 0.8;

				bool conflicting = false;
				conflicting = conflicting ||(candidate.x - min.x) < 0.5 * (wyz + 1.0) * spacing * adjust;
				conflicting = conflicting ||(candidate.x - max.x) > 0.5 * (wyz - 1.0) * spacing * adjust;
				conflicting = conflicting || (candidate.y - min.y) < 0.5 * (wxz + 1.0) * spacing * adjust;
				conflicting = conflicting || (candidate.y - max.y) > 0.5 * (wxz - 1.0) * spacing * adjust;
				conflicting = conflicting || (candidate.z - min.z) < 0.5 * (wxy + 1.0) * spacing * adjust;
				conflicting = conflicting || (candidate.z - max.z) > 0.5 * (wxy - 1.0) * spacing * adjust;
				
				return conflicting;
			};

			auto isDistant = [&accepted, spacingSquared, spacing, &isInConflictZone](Point& candidate, Vector3<double>& center){

				if (isInConflictZone(candidate, center)) {
					return false;
				}

			
				for(int i = accepted.size() - 1; i >= 0; i--){
					Point& prev = accepted[i];

					double cc = sqrt(candidate.squaredDistanceTo(center));
					double pc = sqrt(prev.squaredDistanceTo(center));

					 //if (cc > pc + spacing) {
					 //	return true;
					 //}

					//if (cc < pc - spacing) {
					//	return true;
					//}

					auto distanceSquared = candidate.squaredDistanceTo(prev);

					if(distanceSquared < spacingSquared){
						return false;
					}
				}

				return true;
			};

			if(node->isLeaf()){
				std::sort(node->store.begin(), node->store.end(), byCenter);

				vector<Point> rejected;

				for(Point& candidate : node->store){
					bool distant = isDistant(candidate, center);

					if(distant){
						accepted.push_back(candidate);
					} else {
						rejected.push_back(candidate);
					}
				}

				node->points = accepted;
				node->store = rejected;

			}else{
				for(auto child : node->children){
					if(child == nullptr){
						continue;
					}

					std::sort(child->points.begin(), child->points.end(), byCenter);


					vector<Point> rejected;

					for(Point& candidate : child->points){
						bool distant = isDistant(candidate, center);

						if(distant){
							accepted.push_back(candidate);
						}else{
							rejected.push_back(candidate);
						}
					}


					child->points = rejected;
					node->points = accepted;

					if (child->points.size() == 0 && child->store.size() == 0) {
						int childIndex = child->name.at(child->name.size() - 1) - '0';
						child->parent->children[childIndex] = nullptr;
					} else {
						auto abc = child.get();
						writer->writeNode(abc);

						child->clear();
					}

					

					//cout << node->name << ": " << node->points.size() << " & " << node->store.size() << endl;
				}
			}

			node->isSubsampled = true;
			
			//node->points = accepted;

			if(node->parent == nullptr){
				writer->writeNode(node);
			}

			//cout << repeat("  ", level) << node->name << ": " << accepted.size() << endl;
		});

	}

	void loadChunk(string file, shared_ptr<Node> targetNode){
		auto points = loadPoints(file);

		for (Point& point : points) {
			targetNode->add(point);
		}
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
	indexer.spacing = chunks->max.distanceTo(chunks->min) / 100.0;

	auto writer = make_shared<PotreeWriter>(path, chunks->min, chunks->max);
	indexer.writer = writer;

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

	auto processor = [&indexer](shared_ptr<IndexTask> task) {
		
		auto file = task->chunk->file;
		auto node = task->node;

		indexer.loadChunk(file, node);
		indexer.indexNode(node);
	};

	TaskPool<IndexTask> pool(15, processor);

	for(auto chunk : chunks->list){
		auto node = Node::find(indexer.root, chunk->id);

		shared_ptr<IndexTask> task = make_shared<IndexTask>(chunk, node, path);

		pool.addTask(task);
	}

	pool.close();


	indexer.indexNode(indexer.root);

	writer->close();




}
}