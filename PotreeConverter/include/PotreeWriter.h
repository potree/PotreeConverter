
#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <random>


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

struct Subsample {
	vector<Point> subsample;
	vector<Point> remaining;
};

Subsample subsampleLevel(vector<Point>& samples, double spacing, Vector3<double> min, Vector3<double> max) {
	
	Vector3<double> size = max - min;
	double gridSizeD = (size.x / spacing);
	int gridSize = int(gridSizeD);

	random_device rd;
	mt19937 mt(rd());
	uniform_real_distribution<double> random(0.0, 1.0);

	vector<vector<int>> grid(gridSize * gridSize * gridSize);

	// binning of points into cells
	for (int i = 0; i < samples.size(); i++) {

		Point& point = samples[i];

		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		int ux = std::min(gridSize * nx, gridSize - 1.0);
		int uy = std::min(gridSize * ny, gridSize - 1.0);
		int uz = std::min(gridSize * nz, gridSize - 1.0);

		int index = ux + uy * gridSize + uz * gridSize * gridSize;

		vector<int>& cell = grid[index];
		cell.push_back(i);
	}

	// select random point from each cell
	vector<Point> subsample;
	vector<Point> remaining;
	for (auto& cell : grid) {
		if (cell.size() == 0) {
			continue;
		}

		double r = double(cell.size()) * random(mt);
		int selected = cell[int(r)];

		for (int i : cell) {
			if (i == selected) {
				subsample.push_back(samples[selected]);
			} else {
				remaining.push_back(samples[selected]);
			}
		}
	}

	return {subsample, remaining};
}

struct SubsampleData {
	shared_ptr<Points> points = nullptr;
	string nodeName = "";
};

shared_ptr<Points> toBufferData(vector<Point>& subsample, shared_ptr<Chunk> chunk, shared_ptr<Points> pointsInChunk) {

	int numPoints = subsample.size();

	Attributes attributes = pointsInChunk->attributes;

	shared_ptr<Points> points = make_shared<Points>();
	// TODO potential source of error? does it copy or move the referenced data?
	// would be bad if it kept pointing to the reference, even after it is deleted
	points->points = subsample; 
	points->attributes = attributes;
	uint64_t attributeBufferSize = attributes.byteSize * numPoints;
	points->attributeBuffer = make_shared<Buffer>(attributeBufferSize);

	for (int i = 0; i < points->points.size(); i++) {

		Point& point = points->points[i];

		int srcIndex = point.index;
		int destIndex = i;

		uint8_t* attSrc = pointsInChunk->attributeBuffer->dataU8 + (attributes.byteSize * srcIndex);
		uint8_t* attDest = points->attributeBuffer->dataU8 + (attributes.byteSize * destIndex);

		memcpy(attDest, attSrc, attributes.byteSize);

		point.index = destIndex;
	}

	return points;
}

vector<SubsampleData> subsampleLowerLevels(shared_ptr<Chunk> chunk, shared_ptr<Points> pointsInChunk, shared_ptr<Node> chunkRoot) {

	int startLevel = chunkRoot->name.size() - 2;

	string currentName = chunkRoot->name.substr(0, chunkRoot->name.size() - 1);
	vector<Point>& currentSample = chunkRoot->accepted;
	double currentSpacing = chunkRoot->spacing * 2.0;
	auto min = chunk->min;
	auto max = chunk->max;

	vector<SubsampleData> subsamples;

	for (int level = startLevel; level >= 0; level--) {

		Subsample subsample = subsampleLevel(currentSample, currentSpacing, min, max);

		if (level == startLevel) {
			chunkRoot->accepted = subsample.remaining;
		}

		shared_ptr<Points> subsampleBuffer = toBufferData(subsample.subsample, chunk, pointsInChunk);

		SubsampleData subData = { subsampleBuffer, currentName };

		subsamples.push_back(subData);

		currentName = currentName.substr(0, currentName.size() - 1);
		currentSample = subsampleBuffer->points;
		currentSpacing = currentSpacing * 2.0;
	}


	return subsamples;
}

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
	mutex* mtx_test = new mutex();

	int currentByteOffset = 0;
	mutex mtx_byteOffset;

	vector<SubsampleData> lowerLevelSubsamples;

	fstream* fsFile = nullptr;

	PotreeWriter(string targetDirectory, 
		Vector3<double> min, Vector3<double> max, 
		double spacing, double scale, int upperLevels,
		vector<shared_ptr<Chunk>> chunks) {


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

			//if (node->children[childIndex] == nullptr) {
			//	string childName = node->name + to_string(childIndex);
			//	PWNode* child = new PWNode(childName);

			//	node->children[childIndex] = child;
			//}

			node = node->children[childIndex];
		}

		return node;
	}



	void writeChunk(shared_ptr<Chunk> chunk, shared_ptr<Points> points, shared_ptr<Node> chunkRoot) {

		double tStart = now();

		// returns subsamples and removes subsampled points from nodes
		// TODO not happy with passing a pointer here
		auto subsamples = subsampleLowerLevels(chunk, points, chunkRoot);

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
			int numPoints = pair.node->accepted.size() + pair.node->store.size();
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

			for (Point& point : pair.node->accepted) {
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

		//lowerLevelSubsamples.push_back(subsamples);
		lowerLevelSubsamples.insert(
			lowerLevelSubsamples.begin(),
			subsamples.begin(),
			subsamples.end()
		);

		double lockDuration = now() - tLockStart;
		if (lockDuration > 0.1) {
			cout << "long lock duration: " << lockDuration << " s" << endl;
		}

		for (NodePairing& pair : nodes) {
			int numPoints = pair.node->accepted.size() + pair.node->store.size();
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

	void processLowerLevelSubsamples() {

		unordered_map<string, vector<shared_ptr<Points>>> data;

		for (SubsampleData& subsample : lowerLevelSubsamples) {
			data[subsample.nodeName].push_back(subsample.points);
		}

		for (auto it : data) {
			string nodeName = it.first;
			vector<shared_ptr<Points>> batches = it.second;

			vector<int> id = toVectorID(nodeName);
			PWNode* pwNode = findPWNode(id);

			int numPoints = 0;
			for (auto batch : batches) {
				numPoints += batch->points.size();
			}

			Attributes attributes = batches[0]->attributes;

			int bytesPerPoint = 12 + attributes.byteSize;
			int bufferSize = numPoints * bytesPerPoint;
			vector<uint8_t> buffer(bufferSize, 0);
			uint64_t bufferOffset = 0;
			auto min = this->min;
			auto scale = this->scale;

			auto writePoint = [&bufferOffset, &bytesPerPoint, &buffer, &min, &scale](Point& point, shared_ptr<Points> points) {
				int32_t ix = int32_t((point.x - min.x) / scale);
				int32_t iy = int32_t((point.y - min.y) / scale);
				int32_t iz = int32_t((point.z - min.z) / scale);

				memcpy(buffer.data() + bufferOffset + 0, reinterpret_cast<void*>(&ix), sizeof(int32_t));
				memcpy(buffer.data() + bufferOffset + 4, reinterpret_cast<void*>(&iy), sizeof(int32_t));
				memcpy(buffer.data() + bufferOffset + 8, reinterpret_cast<void*>(&iz), sizeof(int32_t));

				int64_t attributeOffset = point.index * points->attributes.byteSize;

				auto attributeTarget = buffer.data() + bufferOffset + 12;
				auto attributeSource = points->attributeBuffer->dataU8 + attributeOffset;
				memcpy(attributeTarget, attributeSource, points->attributes.byteSize);

				bufferOffset += bytesPerPoint;
			};

			for (auto batch : batches) {
				for (Point& point : batch->points) {
					writePoint(point, batch);
				}
			}

			fsFile->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());


			pwNode->numPoints = numPoints;
			pwNode->byteSize = bufferSize;
			pwNode->byteOffset = currentByteOffset;

			currentByteOffset += bufferSize;
		}

	}

	void close() {

		processLowerLevelSubsamples();

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