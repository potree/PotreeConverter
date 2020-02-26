
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

		string octreeFilePath = path + "/octree.bin";
		fout.open(octreeFilePath, ios::out | ios::binary | ios::app);
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
	

	void PotreeWriter::writeNode(Node* node){

		auto writerNode = findOrCreateWriterNode(node->name);

		static unordered_map<string, Node*> alreadyDone;
		if (alreadyDone.find(node->name) != alreadyDone.end()) {
			int a = 10;
		}
		alreadyDone[node->name] = node;

		{
			auto numPoints = node->points.size() + node->store.size();
			//auto numPoints = node->numAccepted;

			if (numPoints == 0) {
				int a = 10;
			}

			auto sourceBytesPerPoint = 28;
			auto targetBytesPerPoint = 16;
			auto bufferSize = numPoints * targetBytesPerPoint;
			auto buffer = malloc(bufferSize);
			auto bufferU8 = reinterpret_cast<uint8_t*>(buffer);

			auto min = this->min;
			auto max = this->max;
			auto scale = this->scale;

			auto attributeBuffer = node->attributeBuffer;

			int i = 0;

			auto writePoint = [&i, bufferU8, min, scale, sourceBytesPerPoint, targetBytesPerPoint, &attributeBuffer](Point& point){
				int32_t x = (point.x - min.x) / scale.x;
				int32_t y = (point.y - min.y) / scale.y;
				int32_t z = (point.z - min.z) / scale.z;

				auto destXYZ = reinterpret_cast<int32_t*>(bufferU8 + i * targetBytesPerPoint);
				destXYZ[0] = x;
				destXYZ[1] = y;
				destXYZ[2] = z;

				auto source = attributeBuffer->dataU8 + i * sourceBytesPerPoint;

				vector<uint8_t> viewS(attributeBuffer->dataU8, attributeBuffer->dataU8 + 28);

				bufferU8[i * targetBytesPerPoint + 12] = source[24];
				bufferU8[i * targetBytesPerPoint + 13] = source[25];
				bufferU8[i * targetBytesPerPoint + 14] = source[26];
				bufferU8[i * targetBytesPerPoint + 15] = 255;

				i++;
			};

			for(Point& point : node->points){
				writePoint(point);
			}

			for(Point& point : node->store){
				writePoint(point);
			}

			{
				lock_guard<mutex> lock(mtx_writeFile);
				fout.write(reinterpret_cast<char*>(buffer), bufferSize);

				writerNode->byteOffset = this->bytesWritten;
				writerNode->byteSize = bufferSize;
				writerNode->numPoints = numPoints;
				this->bytesWritten += bufferSize;
			}

			free(buffer);

			node->isFlushed = true;
		}

	}

	void PotreeWriter::finishOctree(){
		fout.close();
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

