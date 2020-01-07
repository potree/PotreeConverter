
#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <filesystem>
#include <fstream>

#include "json.hpp"

using json = nlohmann::json;
using namespace std;

namespace fs = std::experimental::filesystem;

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

	mutex mtx_writeChunk;
	int currentByteOffset = 0;

	PotreeWriter(string targetDirectory, Vector3<double> min, Vector3<double> max, double spacing, double scale, int upperLevels) {
		this->targetDirectory = targetDirectory;
		this->min = min;
		this->max = max;
		this->scale = scale;
		this->upperLevels = upperLevels;

		fs::create_directories(targetDirectory);

		pathData = targetDirectory + "/octree.data";
		pathCloudJs = targetDirectory + "/cloud.json";
		pathHierarchy = targetDirectory + "/hierarchy.json";

		fs::remove(pathData);

		root = new PWNode("r");
	}

	struct ChildParams {
		Vector3<double> min;
		Vector3<double> max;
		Vector3<double> size;
		int id;
	};

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

	PWNode* findCreatePWNode(vector<int> id) {

		PWNode* node = root;

		for (int childIndex : id) {

			if (node->children[childIndex] == nullptr) {
				string childName = node->name + to_string(childIndex);
				PWNode* child = new PWNode(childName);

				node->children[childIndex] = child;
			}

			node = node->children[childIndex];
		}

		return node;
	}

	void writeChunk(Chunk* chunk, Node* chunkRoot) {

		double tStart = now();
		lock_guard<mutex> lock(mtx_writeChunk);

		double lockDuration = now() - tStart;

		if (lockDuration > 0.1) {
			cout << "significant lock time in writeChunk(): " << lockDuration << "s " << endl;
		}

		vector<int> id = computeNodeID(chunkRoot);
		PWNode* pwChunkRoot = findCreatePWNode(id);
		pwNodes[chunkRoot] = pwChunkRoot;

		auto& pwNodes = this->pwNodes;

		function<vector<Node*>(Node*, PWNode *, vector<Node*> & nodes)> flatten = [&flatten, &pwNodes](Node* node, PWNode* target, vector<Node*>& nodes) {
			nodes.push_back(node);

			target->numPoints = node->accepted.size() + node->store.size();

			for (int i = 0; i < node->children.size(); i++) {
				
				Node* child = node->children[i];

				if (child == nullptr) {
					continue;
				}

				string childName = target->name + to_string(i);
				PWNode* pwChild = new PWNode(childName);
				target->children[i] = pwChild;
				pwNodes[child] = pwChild;

				flatten(child, pwChild, nodes);
			}

			return nodes;
		};

		vector<Node*> nodes;
		flatten(chunkRoot, pwChunkRoot, nodes);

		Buffer* attributeBuffer = chunk->points->attributeBuffer;
		int64_t attributeBytes = 4;
		const char* ccAttributeBuffer = attributeBuffer->dataChar;

		auto min = this->min;
		auto scale = this->scale;

		uint64_t bufferSize = 0;
		int bytesPerPoint = 16;

		for (Node* node : nodes) {
			int numPoints = node->accepted.size() + node->store.size();
			int nodeBufferSize = numPoints * bytesPerPoint;

			bufferSize += nodeBufferSize;
		}

		vector<uint8_t> buffer(bufferSize, 0);
		uint64_t bufferOffset = 0;

		auto writePoint = [&bufferOffset, &bytesPerPoint , &buffer, &min, &scale, &attributeBytes, &attributeBuffer](Point& point) {
			int32_t ix = int32_t((point.x - min.x) / scale);
			int32_t iy = int32_t((point.y - min.y) / scale);
			int32_t iz = int32_t((point.z - min.z) / scale);

			memcpy(buffer.data() + bufferOffset + 0, reinterpret_cast<void*>(&ix), sizeof(int32_t));
			memcpy(buffer.data() + bufferOffset + 4, reinterpret_cast<void*>(&iy), sizeof(int32_t));
			memcpy(buffer.data() + bufferOffset + 8, reinterpret_cast<void*>(&iz), sizeof(int32_t));

			//file.write(reinterpret_cast<const char*>(&ix), 4);
			//file.write(reinterpret_cast<const char*>(&iy), 4);
			//file.write(reinterpret_cast<const char*>(&iz), 4);

			int64_t attributeOffset = point.index * attributeBytes;
			//file.write(ccAttributeBuffer + attributeOffset, attributeBytes);

			auto attributeTarget = buffer.data() + bufferOffset + 12;
			auto attributeSource = attributeBuffer->dataU8 + attributeOffset;
			memcpy(attributeTarget, attributeSource, attributeBytes);

			bufferOffset += bytesPerPoint;
		};

		for (Node* node : nodes) {

			for (Point& point : node->accepted) {
				writePoint(point);
			}

			for (Point& point : node->store) {
				writePoint(point);
			}

			int numPoints = node->accepted.size() + node->store.size();
			int currentByteSize = numPoints * bytesPerPoint;

			PWNode* pwNode = pwNodes[node];
			pwNode->byteOffset = currentByteOffset;
			pwNode->byteSize = currentByteSize;

			currentByteOffset += currentByteSize;

			delete node;
		}


		fstream file;
		file.open(pathData, ios::out | ios::binary | ios::app);

		file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

		file.close();

	}

	void close() {
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