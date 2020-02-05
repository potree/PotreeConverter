
#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <random>
#include <unordered_map>
#include <vector>
#include <algorithm>


#include "json.hpp"

using json = nlohmann::json;
using namespace std;

namespace fs = std::filesystem;

struct PWNode {

	string name = "";
	int64_t numPoints = 0;
	vector<PWNode*> children;

	int64_t byteOffset = 0;
	int64_t byteSize = 0;

	PWNode(string name) {
		this->name = name;
		this->children.resize(8, nullptr);
	}

};

class PotreeWriter {
public:

	string targetDirectory = "";
	string pathData = "";
	string pathCloudJs = "";
	string pathHierarchy = "";

	Vector3<double> min;
	Vector3<double> max;
	double scale = 1.0;
	double spacing = 1.0;
	int upperLevels = 1;

	PWNode* root = nullptr;
	unordered_map<Node*, PWNode*> pwNodes;

	struct UpperLevelStuff {
		shared_ptr<Node> node;
		shared_ptr<Points> data;
	};

	// partial sampling results for upper levels
	// will contain multiple entries for the same node
	// that need to be merged.
	vector<UpperLevelStuff> upperLevelsResults;

	mutex mtx_writeChunk;
	mutex* mtx_test = new mutex();

	int currentByteOffset = 0;
	mutex mtx_byteOffset;

	Attributes attributes;
	fstream* fsFile = nullptr;

	PotreeWriter(string targetDirectory, 
		Vector3<double> min, Vector3<double> max, 
		double spacing, double scale, int upperLevels,
		vector<shared_ptr<Chunk>> chunks, Attributes attributes): attributes(attributes) {


		this->targetDirectory = targetDirectory;
		this->min = min;
		this->max = max;
		this->spacing = spacing;
		this->scale = scale;
		this->upperLevels = upperLevels;

		fs::create_directories(targetDirectory);

		pathData = targetDirectory + "/octree.data";
		pathCloudJs = targetDirectory + "/cloud.json";
		pathHierarchy = targetDirectory + "/hierarchy.json";

		fs::remove(pathData);

		root = new PWNode("r");

		vector<string> nodeIDs;
		for (auto chunk : chunks) {
			nodeIDs.push_back(chunk->id);
		}
		createNodes(nodeIDs);
	}

	struct ChildParams {
		Vector3<double> min;
		Vector3<double> max;
		Vector3<double> size;
		int id;
	};

	uint64_t increaseByteOffset(uint64_t amount) {

		lock_guard<mutex> lock(mtx_byteOffset);

		uint64_t old = currentByteOffset;

		currentByteOffset += amount;

		return old;
	}

	ChildParams computeChildParameters(Vector3<double>& min, Vector3<double>& max, Vector3<double> point){

		auto size = max - min;

		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		Vector3<double> childMin;
		Vector3<double> childMax;
		Vector3<double> center = min + size / 2.0;

		int childIndex = 0;

		if (nx > 0.5) {
			childIndex = childIndex | 0b100;
			childMin.x = center.x;
			childMax.x = max.x;
		} else {
			childMin.x = min.x;
			childMax.x = center.x;
		}

		if (ny > 0.5) {
			childIndex = childIndex | 0b010;
			childMin.y = center.y;
			childMax.y = max.y;
		} else {
			childMin.y = min.y;
			childMax.y = center.y;
		}

		if (nz > 0.5) {
			childIndex = childIndex | 0b001;
			childMin.z = center.z;
			childMax.z = max.z;
		} else {
			childMin.z = min.z;
			childMax.z = center.z;
		}

		ChildParams params;
		params.min = childMin;
		params.max = childMax;
		params.size = childMax - childMin;
		params.id = childIndex;

		return params;
	}

	vector<int> toVectorID(string stringID) {
		vector<int> id;

		for (int i = 1; i < stringID.size(); i++) {

			int index = stringID[i] - '0'; // ... ouch

			id.push_back(index);
		}

		return id;
	}

	vector<int> computeNodeID(Node* node) {

		auto min = this->min;
		auto max = this->max;
		auto target = (node->min + node->max) / 2.0;

		vector<int> id;

		for (int i = 0; i < upperLevels; i++) {
			auto childParams = computeChildParameters(min, max, target);

			id.push_back(childParams.id);

			min = childParams.min;
			max = childParams.max;
		}

		return id;
	}

	void createNodes(vector<string> nodeIDs) {

		for (string nodeID : nodeIDs) {
			PWNode* node = root;

			vector<int> id = toVectorID(nodeID);

			for (int childIndex : id) {

				if (node->children[childIndex] == nullptr) {
					string childName = node->name + to_string(childIndex);
					PWNode* child = new PWNode(childName);

					node->children[childIndex] = child;
				}

				node = node->children[childIndex];
			}
		}

	}

	PWNode* findPWNode(vector<int> id) {

		PWNode* node = root;

		for (int childIndex : id) {
			node = node->children[childIndex];

			if (node == nullptr) {
				return nullptr;
			}
		}

		return node;
	}



	void writeChunk(shared_ptr<Chunk> chunk, shared_ptr<Points> points, ProcessResult processResult) {

		double tStart = now();

		{
			lock_guard<mutex> lock(mtx_writeChunk);

			UpperLevelStuff stuff = { processResult.upperLevels , processResult.upperLevelsData };
			upperLevelsResults.push_back(stuff); 
		}

		auto chunkRoot = processResult.chunkRoot;

		if (chunkRoot == nullptr) {
			return;
		}

		struct NodePairing {
			shared_ptr<Node> node = nullptr;
			PWNode* pwNode = nullptr;

			NodePairing(shared_ptr<Node> node, PWNode* pwNode) {
				this->node = node;
				this->pwNode = pwNode;
			}
		};

		function<void(shared_ptr<Node>, PWNode*, vector<NodePairing> & nodes)> flatten = [&flatten](shared_ptr<Node> node, PWNode* pwNode, vector<NodePairing>& nodes) {
			nodes.emplace_back(node, pwNode);

			//for (int i = 0; i < node->children.size(); i++) {
			
				//auto child = node->children[i];
			for(auto child : node->children){

				if (child == nullptr) {
					continue;
				}

				PWNode* pwChild = new PWNode(child->name);
				pwNode->children.push_back(pwChild);

				flatten(child, pwChild, nodes);
			}

			return nodes;
		};

		PWNode* pwChunkRoot = new PWNode(chunkRoot->name);
		vector<NodePairing> nodes;
		flatten(chunkRoot, pwChunkRoot, nodes);

		Attributes attributes = points->attributes;
		auto attributeBuffer = points->attributeBuffer;
		const char* ccAttributeBuffer = attributeBuffer->dataChar;

		auto min = this->min;
		auto scale = this->scale;

		uint64_t bufferSize = 0;
		int bytesPerPoint = 12 + attributes.byteSize;

		for (NodePairing pair : nodes) {
			int numPoints = pair.node->grid->accepted.size() + pair.node->store.size();
			int nodeBufferSize = numPoints * bytesPerPoint;

			bufferSize += nodeBufferSize;
		}

		vector<uint8_t> buffer(bufferSize, 0);
		uint64_t bufferOffset = 0;

		auto writePoint = [&bufferOffset, &bytesPerPoint , &buffer, &min, &scale, &attributes, &attributeBuffer](Point& point) {
			int32_t ix = int32_t((point.x - min.x) / scale);
			int32_t iy = int32_t((point.y - min.y) / scale);
			int32_t iz = int32_t((point.z - min.z) / scale);

			memcpy(buffer.data() + bufferOffset + 0, reinterpret_cast<void*>(&ix), sizeof(int32_t));
			memcpy(buffer.data() + bufferOffset + 4, reinterpret_cast<void*>(&iy), sizeof(int32_t));
			memcpy(buffer.data() + bufferOffset + 8, reinterpret_cast<void*>(&iz), sizeof(int32_t));

			int64_t attributeOffset = point.index * attributes.byteSize;

			auto attributeTarget = buffer.data() + bufferOffset + 12;
			auto attributeSource = attributeBuffer->dataU8 + attributeOffset;
			memcpy(attributeTarget, attributeSource, attributes.byteSize);

			bufferOffset += bytesPerPoint;
		};


		for (NodePairing& pair: nodes) {
			for (Point& point : pair.node->grid->accepted) {
				writePoint(point);
			}

			for (Point& point : pair.node->store) {
				writePoint(point);
			}
		}


		// ==============================================================================
		// FROM HERE ON, ONLY ONE THREAD UPDATES THE HIERARCHY DATA AND WRITES TO FILE
		// ==============================================================================

		double tLockStart = now();
		lock_guard<mutex> lock(mtx_writeChunk);

		double lockDuration = now() - tLockStart;
		if (lockDuration > 0.1) {
			cout << "long lock duration: " << lockDuration << " s" << endl;
		}

		for (NodePairing& pair : nodes) {
			int numPoints = pair.node->grid->accepted.size() + pair.node->store.size();
			int nodeBufferSize = numPoints * bytesPerPoint;

			pair.pwNode->byteOffset = currentByteOffset;
			pair.pwNode->byteSize = nodeBufferSize;
			pair.pwNode->numPoints = numPoints;

			currentByteOffset += nodeBufferSize;
		}

		// attach local chunk-root to global hierarchy, by replacing previously created dummy
		vector<int> pwid = toVectorID(pwChunkRoot->name);
		PWNode* pwMain = findPWNode(pwid);
		PWNode* pwLocal = pwChunkRoot;

		pwMain->byteOffset = pwLocal->byteOffset;
		pwMain->byteSize = pwLocal->byteSize;
		pwMain->children = pwLocal->children;
		pwMain->name = pwLocal->name;
		pwMain->numPoints = pwLocal->numPoints;

		// now write everything to file
		if (fsFile == nullptr) {
			fsFile = new fstream();
			fsFile->open(pathData, ios::out | ios::binary | ios::app);
		}

		fsFile->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

		// fsFile is closed in PotreeWriter::close()
	}

	void processUpperLevelResults() {

		auto results = upperLevelsResults;

		struct NodeAndData {
			Node* node;
			shared_ptr<Points> data;
		};

		unordered_map<string, vector<NodeAndData>> nodes;

		for (auto stuff : results) {
			stuff.node->traverse([&nodes, &stuff](Node* node){

				NodeAndData nad;
				nad.node = node;
				nad.data = stuff.data;

				nodes[node->name].push_back(nad);
			});
		}

		for (auto it : nodes) {

			string name = it.first;
			auto id = toVectorID(name);
			PWNode* pwNode = findPWNode(id);

			if (pwNode == nullptr) {
				cout << "ERROR: points attempted to be added in an unintented chunk. " << endl;
				continue;
			}

			vector<NodeAndData>& nodeParts = it.second;

			int numPoints = 0;
			for (NodeAndData& part : nodeParts) {
				numPoints += part.node->grid->accepted.size(); // shouldn't have any stores
			}

			int bytesPerPoint = 12 + attributes.byteSize;
			uint64_t bufferSize = numPoints * bytesPerPoint;

			vector<uint8_t> buffer(bufferSize, 0);
			uint64_t bufferOffset = 0;

			auto min = this->min;
			auto max = this->max;
			auto scale = this->scale;
			auto attributes = this->attributes;

			for (auto nad : nodeParts) {

				auto node = nad.node;
				auto srcBuffer = nad.data->attributeBuffer;
			
				auto writePoint = [&bufferOffset, &bytesPerPoint, &buffer, &min, &scale, &attributes, srcBuffer](Point& point) {
					int32_t ix = int32_t((point.x - min.x) / scale);
					int32_t iy = int32_t((point.y - min.y) / scale);
					int32_t iz = int32_t((point.z - min.z) / scale);

					memcpy(buffer.data() + bufferOffset + 0, reinterpret_cast<void*>(&ix), sizeof(int32_t));
					memcpy(buffer.data() + bufferOffset + 4, reinterpret_cast<void*>(&iy), sizeof(int32_t));
					memcpy(buffer.data() + bufferOffset + 8, reinterpret_cast<void*>(&iz), sizeof(int32_t));

					int64_t attributeOffset = point.index * attributes.byteSize;

					auto attributeTarget = buffer.data() + bufferOffset + 12;
					auto attributeSource = srcBuffer->dataU8 + attributeOffset;
					memcpy(attributeTarget, attributeSource, attributes.byteSize);

					bufferOffset += bytesPerPoint;
				};

				for (Point& point : node->grid->accepted) {
					writePoint(point);
				}
			}


			pwNode->byteOffset = currentByteOffset;
			pwNode->byteSize = bufferSize;
			pwNode->numPoints = numPoints;

			currentByteOffset += bufferSize;

			fsFile->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
		}

	}

	void close() {

		processUpperLevelResults();

		fsFile->close();

		writeHierarchy();
		writeCloudJson();

	}

	void writeCloudJson() {

		auto min = this->min;
		auto max = this->max;

		json box = {
			{"min", {min.x, min.y, min.z}},
			{"max", {max.x, max.y, max.z}},
		};

		json aPosition = {
			{"name", "position"},
			{"elements", 3},
			{"elementSize", 4},
			{"type", "int32"},
		};

		json aRGBA = {
			{"name", "rgba"},
			{"elements", 4},
			{"elementSize", 1},
			{"type", "uint8"},
		};

		json attributes = {
			{"bla", "blubb"}
		};

		json js = {
			{"version", "1.9"},
			{"projection", ""},
			{"boundingBox", box},
			{"spacing", spacing},
			{"scale", scale},
			{"attributes", {aPosition, aRGBA}},
		};


		{
			string str = js.dump(4);

			fstream file;
			file.open(pathCloudJs, ios::out);

			file << str;

			file.close();
		}
	}

	void writeHierarchy() {

		// for debugging/testing
		writeHierarchyJSON();

		writeHierarchyBinary();




	}

	void writeHierarchyBinary() {

		vector<PWNode*> nodes;
		function<void(PWNode*)> traverse = [&traverse, &nodes](PWNode* node){
			nodes.push_back(node);

			for (auto child : node->children) {
				if (child != nullptr) {
					traverse(child);
				}
			}
		};
		traverse(root);

		// sizeof(NodeData) = 32 bytes
		struct NodeData {
			uint64_t byteOffset = 0; // location of first byte in data store
			uint64_t byteLength = 0; // byte size in data store
			uint64_t childPosition = 0; // location of first child in hierarchy
			uint8_t childBitset = 0; // which of the eight children exist?
		};

		// sort in breadth-first order
		auto compare = [](PWNode* a, PWNode* b) -> bool {
			if (a->name.size() == b->name.size()) {
				bool result = lexicographical_compare(
					a->name.begin(), a->name.end(),
					b->name.begin(), b->name.end());

				return result;
			} else {
				return a->name.size() < b->name.size();
			}
		};

		sort(nodes.begin(), nodes.end(), compare);

		unordered_map<string, uint64_t> nodesMap;
		vector<NodeData> nodesData(nodes.size());

		for (uint64_t i = 0; i < nodes.size(); i++) {

			PWNode* node = nodes[i];
			NodeData& nodeData = nodesData[i];

			nodeData.byteOffset = node->byteOffset;
			nodeData.byteLength = node->byteSize;

			nodesMap[node->name] = i;

			if (node->name != "r") {
				string parentName = node->name.substr(0, node->name.size() - 1);
				uint64_t parentIndex = nodesMap[parentName];
				PWNode* parent = nodes[parentIndex];
				NodeData& parentData = nodesData[parentIndex];


				int index = node->name.at(node->name.size() - 1) - '0';
				int bitmask = 1 << index;
				parentData.childBitset = parentData.childBitset | bitmask;

				if (parentData.childPosition == 0) {
					parentData.childPosition = i;
				}

			}
		}

		cout << "#nodes: " << nodes.size() << endl;

		{
			string jsPath = targetDirectory + "/hierarchy.bin";

			fstream file;
			file.open(jsPath, ios::out | ios::binary);

			char* data = reinterpret_cast<char*>(nodesData.data());
			file.write(data, nodesData.size() * sizeof(NodeData));

			cout << "sizeof(NodeData): " << sizeof(NodeData) << endl;

			//for (int i = 0; i < 109; i++) {
			//	NodeData& nodeData = nodesData[i];
			//	PWNode* node = nodes[i];

			//	file << "=================" << endl;
			//	file << "position; " << i << endl;
			//	file << "name: " << node->name << endl;
			//	file << "offset: " << nodeData.byteOffset << endl;
			//	file << "size: " << nodeData.byteLength << endl;
			//	file << "childPosition: " << nodeData.childPosition << endl;
			//	file << "children: ";

			//	for (int j = 0; j < 8; j++) {
			//		int value = nodeData.childBitset & (1 << j);

			//		file << (value > 0 ? 1 : 0)<< ", ";
			//	}
			//	file << endl;


			//}
			

			file.close();
		}


	}

	void writeHierarchyJSON() {

		function<json(PWNode*)> traverse = [&traverse](PWNode* node) -> json {

			vector<json> jsChildren;
			for (PWNode* child : node->children) {
				if (child == nullptr) {
					continue;
				}

				json jsChild = traverse(child);
				jsChildren.push_back(jsChild);
			}

			uint64_t numPoints = node->numPoints;
			int64_t byteOffset = node->byteOffset;
			int64_t byteSize = node->byteSize;

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

		{ // write to file
			string str = js.dump(4);

			string jsPath = pathHierarchy;

			fstream file;
			file.open(jsPath, ios::out);

			file << str;

			file.close();
		}
	}



};