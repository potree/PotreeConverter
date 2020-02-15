
#include "./modules/index_bluenoise/PotreeWriter.h"

#include <mutex>
#include <queue>
#include <fstream>
#include <sstream>

#include "json.hpp"

#include "converter_utils.h"

using std::mutex;
using std::lock_guard;
using std::deque;
using std::fstream;
using std::ios;
using std::stringstream;

using json = nlohmann::json;


namespace bluenoise {


	mutex mtx_findNode;
	mutex mtx_writeFile;



	PotreeWriter::PotreeWriter(string path, Vector3<double> min, Vector3<double> max) {
		this->path = path;
		this->min = min;
		this->max = max;

		root = make_shared<WriterNode>("r", min, max);
	}

	PotreeWriter::~PotreeWriter(){
		close();
	}

	shared_ptr<WriterNode> PotreeWriter::findOrCreateWriterNode(string name){

		lock_guard<mutex> lock(mtx_findNode);

		auto node = root;

		for(int i = 1; i < name.size(); i++){
			int childIndex = name.at(i) - '0';

			auto child = node->children[childIndex];

			if(child == nullptr){

				string childName = name.substr(0, i + 1);
				auto box = childBoundingBoxOf(node->min, node->max, childIndex);

				child = make_shared<WriterNode>(childName, box.min, box.max);

				node->children[childIndex] = child;
			}

			node = child;
		}

		return node;
	}
	

	void PotreeWriter::writeChunk(shared_ptr<IndexedChunk> chunk){

		string octreeFilePath = path + "/octree.bin";

		auto localWriteRoot = findOrCreateWriterNode(chunk->chunk->id);
		auto localIndexRoot = chunk->root;

		struct NodePair{
			shared_ptr<Node> node;
			shared_ptr<WriterNode> writerNode;
		};

		vector<NodePair> nodePairs = {{localIndexRoot, localWriteRoot}};
		deque<NodePair> stack;
		stack.push_back({localIndexRoot, localWriteRoot});

		while(!stack.empty()){
			auto pair = stack.back();
			stack.pop_back();
		
			// for(auto child : pair.node->children){
			for(int childIndex = 0; childIndex < 8; childIndex++){
				auto child = pair.node->children[childIndex];

				if(child == nullptr){
					continue;
				}

				auto writerChild = make_shared<WriterNode>(child->name, child->min, child->max);
				writerChild->numPoints = child->points.size() + child->store.size();

				pair.writerNode->children[childIndex] = writerChild;

				stack.push_back({child, writerChild});
				nodePairs.push_back({child, writerChild});
			}
		}

		lock_guard<mutex> lock(mtx_writeFile);

		fstream fout;
		fout.open(octreeFilePath, ios::out | ios::binary | ios::app);

		for(auto pair : nodePairs){
			auto node = pair.node;
			auto writerNode = pair.writerNode;

			auto numPoints = node->points.size() + node->store.size();

			auto bytesPerPoint = 16;
			auto bufferSize = numPoints * bytesPerPoint;
			auto buffer = malloc(bufferSize);
			auto bufferU8 = reinterpret_cast<uint8_t*>(buffer);

			auto min = this->min;
			auto max = this->max;
			auto scale = this->scale;

			int i = 0;

			auto writePoint = [&i, bufferU8, min, scale, bytesPerPoint](Point& point){
				int32_t x = (point.x - min.x) / scale.x;
				int32_t y = (point.y - min.y) / scale.y;
				int32_t z = (point.z - min.z) / scale.z;

				auto destXYZ = reinterpret_cast<int32_t*>(bufferU8 + i * bytesPerPoint);
				destXYZ[0] = x;
				destXYZ[1] = y;
				destXYZ[2] = z;

				bufferU8[i * bytesPerPoint + 12] = 255;
				bufferU8[i * bytesPerPoint + 13] = 0;
				bufferU8[i * bytesPerPoint + 14] = 0;
				bufferU8[i * bytesPerPoint + 15] = 255;

				i++;
			};

			for(Point& point : node->points){
				writePoint(point);
			}

			for(Point& point : node->store){
				writePoint(point);
			}

			fout.write(reinterpret_cast<char*>(buffer), bufferSize);

			writerNode->byteOffset = bytesWritten;
			writerNode->byteSize = bufferSize;

			bytesWritten += bufferSize;

			free(buffer);
		}

		fout.close();



	}

	void PotreeWriter::finishOctree(){

	}

	void PotreeWriter::writeMetadata(){

		json js;

		js["name"] = "abc";
		js["boundingBox"]["min"] = {min.x, min.y, min.z};
		js["boundingBox"]["max"] = {max.x, max.y, max.z};
		js["projection"] = "";
		js["description"] = "";
		js["points"] = 123456;
		js["scale"] = 0.001;

		json jsAttributePosition;
		jsAttributePosition["name"] = "position";
		jsAttributePosition["type"] = "int32";
		jsAttributePosition["scale"] = "0.001";
		jsAttributePosition["offset"] = "3.1415";

		json jsAttributeColor;
		jsAttributeColor["name"] = "color";
		jsAttributeColor["type"] = "uint8";

		js["attributes"] = {jsAttributePosition, jsAttributeColor};
		

		string str = js.dump(4);

		string metadataPath = path + "/metadata.json";

		writeFile(metadataPath, str);

	}

	void PotreeWriter::writeHierarchy(){

		function<json(shared_ptr<WriterNode>)> traverse = [&traverse](shared_ptr<WriterNode> node) -> json {

			vector<json> jsChildren;
			for (auto child : node->children) {
				if (child == nullptr) {
					continue;
				}

				json jsChild = traverse(child);
				jsChildren.push_back(jsChild);
			}

			// uint64_t numPoints = node->numPoints;
			int64_t byteOffset = node->byteOffset;
			int64_t byteSize = node->byteSize;
			int64_t numPoints = node->numPoints;

			json jsNode = {
				{"name", node->name},
				{"numPoints", numPoints},
				{"byteOffset", byteOffset},
				{"byteSize", byteSize},
				{"children", jsChildren}
			};

			return jsNode;
		};

		json js;
		js["hierarchy"] = traverse(root);



		string str = js.dump(4);

		string hierarchyPath = path + "/hierarchy.json";

		writeFile(hierarchyPath, str);

	}

	void PotreeWriter::close(){
		if(closed){
			return;
		}

		finishOctree();
		writeMetadata();
		writeHierarchy();
	}



}

