
#include <cerrno>
#include <execution>
#include <algorithm>

#include "indexer_poissondisk.h"

#include "Attributes.h"

using std::unique_lock;

namespace indexer_poissondisk {

	struct Point {
		double x;
		double y;
		double z;
		int32_t pointIndex;
		int32_t childIndex;
	};


	mutex mtx_debug;
	double debug = 0.0;







	inline int childIndexOf(Vector3& min, Vector3& max, Vector3& point) {
		int childIndex = 0;

		double nx = (point.x - min.x) / (max.x - min.x);
		double ny = (point.y - min.y) / (max.x - min.x);
		double nz = (point.z - min.z) / (max.x - min.x);

		if (nx > 0.5) {
			childIndex = childIndex | 0b100;
		}

		if (ny > 0.5) {
			childIndex = childIndex | 0b010;
		}

		if (nz > 0.5) {
			childIndex = childIndex | 0b001;
		}

		return childIndex;
	}

	inline int computeChildIndex(BoundingBox box, Vector3& point) {
		return childIndexOf(box.min, box.max, point);
	}

	shared_ptr<Chunks> getChunks(string pathIn) {
		string chunkDirectory = pathIn + "/chunks";

		string metadataText = readTextFile(chunkDirectory + "/metadata.json");
		json js = json::parse(metadataText);

		Vector3 min = {
			js["min"][0].get<double>(),
			js["min"][1].get<double>(),
			js["min"][2].get<double>()
		};

		Vector3 max = {
			js["max"][0].get<double>(),
			js["max"][1].get<double>(),
			js["max"][2].get<double>()
		};

		vector<Attribute> attributeList;
		auto jsAttributes = js["attributes"];
		for (auto jsAttribute : jsAttributes) {

			string name = jsAttribute["name"];
			string description = jsAttribute["description"];
			int size = jsAttribute["size"];
			int numElements = jsAttribute["numElements"];
			int elementSize = jsAttribute["elementSize"];
			AttributeType type = typenameToType(jsAttribute["type"]);

			Attribute attribute(name, size, numElements, elementSize, type);

			attributeList.push_back(attribute);
		}

		double scaleX = js["scale"][0];
		double scaleY = js["scale"][1];
		double scaleZ = js["scale"][2];

		double offsetX = js["offset"][0];
		double offsetY = js["offset"][1];
		double offsetZ = js["offset"][2];

		Attributes attributes(attributeList);
		attributes.posScale = { scaleX, scaleY, scaleZ };
		attributes.posOffset = { offsetX, offsetY, offsetZ };
		

		auto toID = [](string filename) -> string {
			string strID = stringReplace(filename, "chunk_", "");
			strID = stringReplace(strID, ".bin", "");

			return strID;
		};

		vector<shared_ptr<Chunk>> chunksToLoad;
		for (const auto& entry : fs::directory_iterator(chunkDirectory)) {
			string filename = entry.path().filename().string();
			string chunkID = toID(filename);

			//if (chunkID != "r464") {
			//	continue;
			//}

			// if (chunkID != "r0776767") {
			// // if (chunkID != "r077632") {
			// 	continue;
			// }

			if (!iEndsWith(filename, ".bin")) {
				continue;
			}

			shared_ptr<Chunk> chunk = make_shared<Chunk>();
			chunk->file = entry.path().string();
			chunk->id = chunkID;

			BoundingBox box = { min, max };

			for (int i = 1; i < chunkID.size(); i++) {
				int index = chunkID[i] - '0'; // this feels so wrong...

				box = childBoundingBoxOf(box.min, box.max, index);
			}

			chunk->min = box.min;
			chunk->max = box.max;

			chunksToLoad.push_back(chunk);

			//break;
		}



		auto chunks = make_shared<Chunks>(chunksToLoad, min, max);
		chunks->attributes = attributes;

		return chunks;
	}



	Node::Node(string name, Vector3 min, Vector3 max) {
		this->name = name;
		this->min = min;
		this->max = max;

		children.resize(8, nullptr);
	}

	void Node::traverse(function<void(Node*)> callback) {
		callback(this);

		for (auto child : children) {

			if (child != nullptr) {
				child->traverse(callback);
			}

		}
	}

	void Node::addDescendant(shared_ptr<Node> descendant) {

		static mutex mtx;
		lock_guard<mutex> lock(mtx);

		int descendantLevel = descendant->name.size() - 1;

		Node* current = this;

		for (int level = 1; level < descendantLevel; level++) {
			int index = descendant->name[level] - '0';

			if (current->children[index] != nullptr) {
				current = current->children[index].get();
			} else {
				string childName = current->name + to_string(index);
				auto box = childBoundingBoxOf(current->min, current->max, index);

				auto child = make_shared<Node>(childName, box.min, box.max);

				current->children[index] = child;

				current = child.get();
			}
		}

		auto index = descendant->name[descendantLevel] - '0';
		current->children[index] = descendant;

	}

void Node::traversePost(function<void(Node*)> callback) {
	for (auto child : children) {

		if (child != nullptr) {
			child->traversePost(callback);
		}
	}

	callback(this);
}


struct NodeCandidate {
	string name = "";
	int64_t indexStart = 0;
	int64_t numPoints = 0;
	int64_t level = 0;
	int64_t x = 0;
	int64_t y = 0;
	int64_t z = 0;
};

vector<vector<int64_t>> createSumPyramid(vector<int64_t>& grid, int gridSize) {

	auto tStart = now();

	int maxLevel = std::log2(gridSize);
	int currentGridSize = gridSize / 2;

	vector<vector<int64_t>> sumPyramid(maxLevel + 1);
	for (int level = 0; level < maxLevel; level++) {
		auto cells = pow(8, level);
		sumPyramid[level].resize(cells, 0);
	}
	sumPyramid[maxLevel] = grid;

	for (int level = maxLevel - 1; level >= 0; level--) {

		for (int x = 0; x < currentGridSize; x++) {
		for (int y = 0; y < currentGridSize; y++) {
		for (int z = 0; z < currentGridSize; z++) {

			auto index = mortonEncode_magicbits(z, y, x);
			auto index_p1 = mortonEncode_magicbits(2 * z, 2 * y, 2 * x);

			int64_t sum = 0;
			for (int i = 0; i < 8; i++) {
				sum += sumPyramid[level + 1][index_p1 + i];
			}

			if (sum > 0) {
				int a = 10;
			}

			sumPyramid[level][index] = sum;

		}
		}
		}

		currentGridSize = currentGridSize / 2;

	}

	return sumPyramid;
}

vector<NodeCandidate> createNodes(vector<vector<int64_t>>& pyramid) {

	vector<NodeCandidate> nodes;

	//createNodesTraversal(pyramid, 0, 0, 0, 0, &nodes);

	vector<vector<int64_t>> pyramidOffsets;
	for (auto& counters : pyramid) {

		if (counters.size() == 1) {
			pyramidOffsets.push_back({ 0 });
		} else {

			vector<int64_t> offsets(counters.size(), 0);
			for (int64_t i = 1; i < counters.size(); i++) {
				int64_t offset = offsets[i - 1] + counters[i - 1];

				offsets[i] = offset;
			}

			pyramidOffsets.push_back(offsets);
		}
	}

	// pyramid starts at level 0 -> gridSize = 1
	// 2 levels -> levels 0 and 1 -> maxLevel 1
	auto maxLevel = pyramid.size() - 1;

	NodeCandidate root;
	root.name = "";
	root.level = 0;
	root.x = 0;
	root.y = 0;
	root.z = 0;
	//root.indexStart = 0;
	//root.numPoints = pyramid[0][0];

	vector<NodeCandidate> stack = { root };

	while (!stack.empty()) {

		NodeCandidate candidate = stack.back();
		stack.pop_back();

		auto level = candidate.level;
		auto x = candidate.x;
		auto y = candidate.y;
		auto z = candidate.z;

		auto& grid = pyramid[level];
		auto index = mortonEncode_magicbits(z, y, x);
		int64_t numPoints = grid[index];

		if (level == maxLevel) {
			// don't split further at this time. May be split further in another pass

			if (numPoints > 0) {
				nodes.push_back(candidate);
			}
		} else if (numPoints > maxPointsPerChunk) {
			// split (too many points in node)

			for (int i = 0; i < 8; i++) {

				auto index_p1 = mortonEncode_magicbits(2 * z, 2 * y, 2 * x) + i;
				auto count = pyramid[level + 1][index_p1];

				if (count > 0) {
					NodeCandidate child;
					child.level = level + 1;
					child.name = candidate.name + to_string(i);
					child.indexStart = pyramidOffsets[level + 1][index_p1];
					child.numPoints = count;
					child.x = 2 * x + ((i & 0b100) >> 2);
					child.y = 2 * y + ((i & 0b010) >> 1);
					child.z = 2 * z + ((i & 0b001) >> 0);

					stack.push_back(child);

					if (child.name == "75") {
						int a = 10;
					}
				}
			}

		} else if(numPoints > 0 ){
			// accept (small enough)
			nodes.push_back(candidate);
		}

	}

	return nodes;
}

// 1. Counter grid
// 2. Hierarchy from counter grid
// 3. identify nodes that need further refinment
// 4. Recursively repeat at 1. for identified nodes
void buildHierarchy(Indexer* indexer, Node* node, shared_ptr<Buffer> points, int numPoints, int depth) {

	// printElapsedTime("start buildHierarchy: " + to_string(numPoints), 0);

	if (numPoints < maxPointsPerChunk) {
		Node* realization = node;
		realization->indexStart = 0;
		realization->numPoints = numPoints;
		realization->points = points;

		return;
	}


	auto tStart = now();

	int64_t levels = 5; // = gridSize 32
	int64_t counterGridSize = pow(2, levels);
	vector<int64_t> counters(counterGridSize * counterGridSize * counterGridSize, 0);

	auto min = node->min;
	auto max = node->max;
	auto size = max - min;
	auto attributes = indexer->attributes;
	int bpp = attributes.bytes;
	auto scale = attributes.posScale;
	auto offset = attributes.posOffset;

	auto gridIndexOf = [&points, bpp, scale, offset, min, size, counterGridSize](int64_t pointIndex) {

		int64_t pointOffset = pointIndex * bpp;
		int32_t* xyz = reinterpret_cast<int32_t*>(points->data_u8 + pointOffset);

		double x = (xyz[0] * scale.x) + offset.x;
		double y = (xyz[1] * scale.y) + offset.y;
		double z = (xyz[2] * scale.z) + offset.z;

		int64_t ix = double(counterGridSize) * (x - min.x) / size.x;
		int64_t iy = double(counterGridSize) * (y - min.y) / size.y;
		int64_t iz = double(counterGridSize) * (z - min.z) / size.z;

		ix = std::max(int64_t(0), std::min(ix, counterGridSize - 1));
		iy = std::max(int64_t(0), std::min(iy, counterGridSize - 1));
		iz = std::max(int64_t(0), std::min(iz, counterGridSize - 1));

		int64_t index = mortonEncode_magicbits(iz, iy, ix);

		return index;
	};

	// printElapsedTime("start counting", 0);

	// COUNTING
	for (int i = 0; i < numPoints; i++) {
		auto index = gridIndexOf(i);
		counters[index]++;
	}

	// printElapsedTime("start counted", 0);

	// SORTING
	vector<int64_t> offsets(counters.size(), 0);
	for (int i = 1; i < counters.size(); i++) {
		offsets[i] = offsets[i - 1] + counters[i - 1];
	}

	//{ // INPLACE

	//	//printMemoryReport();

	//	size_t pointsize = bpp;
	//	void* pointdata = malloc(pointsize);


	//	for (size_t i = 0; i < numPoints; i++) {
	//		//Point point = data.points[i];
	//		memcpy(pointdata, points->data_u8 + i * pointsize, pointsize);

	//		//auto index = toIndex(point);
	//		auto index = gridIndexOf(i);
	//		auto targetIndex = offsets[index]++;
	//		auto counter = counters[index];

	//		if (i <= targetIndex) {
	//			continue;
	//		}

	//		//data.points[i] = data.points[targetIndex];
	//		memcpy(points->data_u8 + i * pointsize, points->data_u8 + targetIndex * pointsize, pointsize);

	//		//data.points[targetIndex] = point;
	//		memcpy(points->data_u8 + targetIndex * pointsize, pointdata, pointsize);

	//		if (targetIndex != i) {
	//			i--;
	//		}

	//	}

	//	free(pointdata);
	//}

	{ // WITH TEMP COPY

		Buffer tmp(numPoints * bpp);

		for (int i = 0; i < numPoints; i++) {
			auto index = gridIndexOf(i);
			auto targetIndex = offsets[index]++;

			memcpy(tmp.data_u8 + targetIndex * bpp, points->data_u8 + i * bpp, bpp);
		}

		//printMemoryReport();

		memcpy(points->data, tmp.data, numPoints * bpp);
	}


	//auto tStart01 = now();

	auto pyramid = createSumPyramid(counters, counterGridSize);

	auto nodes = createNodes(pyramid);

	//printElapsedTime("01", tStart01);

	auto expandTo = [node](NodeCandidate& candidate) {

		string startName = node->name;
		string fullName = startName + candidate.name;

		// e.g. startName: r, fullName: r031
		// start iteration with char at index 1: "0"

		Node* currentNode = node;
		for (int i = startName.size(); i < fullName.size(); i++) {
			int index = fullName.at(i) - '0';

			if (currentNode->children[index] == nullptr) {
				auto childBox = childBoundingBoxOf(currentNode->min, currentNode->max, index);
				string childName = currentNode->name + to_string(index);

				shared_ptr<Node> child = make_shared<Node>();
				child->min = childBox.min;
				child->max = childBox.max;
				child->name = childName;
				child->children.resize(8);

				currentNode->children[index] = child;
				currentNode = child.get();
			} else {
				currentNode = currentNode->children[index].get();
			}

			
		}

		return currentNode;
	};

	vector<Node*> needRefinement;

	for (NodeCandidate& candidate : nodes) {

		Node* realization = expandTo(candidate);
		realization->indexStart = candidate.indexStart;
		realization->numPoints = candidate.numPoints;
		int64_t bytes = candidate.numPoints * bpp;
		auto buffer = make_shared<Buffer>(bytes);
		memcpy(buffer->data,
			points->data_u8 + candidate.indexStart * bpp,
			candidate.numPoints * bpp
		);

		realization->points = buffer;

		if (realization->numPoints > maxPointsPerChunk) {
			needRefinement.push_back(realization);
		}
	}

	for (auto subject : needRefinement) {
		auto buffer = subject->points;
		auto numPoints = subject->numPoints;

		subject->points = nullptr;
		subject->numPoints = 0;

		buildHierarchy(indexer, subject, buffer, numPoints, depth + 1);
	}

	//if (depth == 0) {
	//	printElapsedTime("buildHierarchy", tStart);
	//}


}


Writer::Writer(Indexer* indexer) {
	this->indexer = indexer;

	string octreePath = indexer->targetDir + "/octree.bin";
	fsOctree.open(octreePath, ios::out | ios::binary);

	launchWriterThread();
}

mutex mtx_backlog;
int64_t Writer::backlogSizeMB() {
	lock_guard<mutex> lock(mtx_backlog);

	int64_t backlogBytes = backlog.size() * capacity;
	int64_t backlogMB = backlogBytes / (1024 * 1024);

	return backlogMB;
}

void Writer::writeAndUnload(Node* node) {

	if (node->alreadyFlushed) {
		return;
	}

	auto attributes = indexer->attributes;

	static int64_t counter = 0;
	counter += node->numPoints;

	int64_t numPoints = node->numPoints;
	int64_t byteSize = numPoints * attributes.bytes;

	node->byteSize = byteSize;

	shared_ptr<Buffer> buffer = nullptr;
	int64_t targetOffset = 0;
	{
		lock_guard<mutex> lock(mtx);

		int64_t byteOffset = indexer->byteOffset.fetch_add(byteSize);
		node->byteOffset = byteOffset;

		if (activeBuffer == nullptr) {
			activeBuffer = make_shared<Buffer>(capacity);
		} else if (activeBuffer->pos + byteSize > capacity) {
			backlog.push_back(activeBuffer);
			activeBuffer = make_shared<Buffer>(capacity);
		}

		buffer = activeBuffer;
		targetOffset = activeBuffer->pos;

		activeBuffer->pos += byteSize;

		memcpy(buffer->data_char + targetOffset, node->points->data_char, byteSize);

		node->points = nullptr;
	}	

}

void Writer::launchWriterThread() {
	thread([&]() {

		while (true) {

			shared_ptr<Buffer> buffer = nullptr;

			{
				lock_guard<mutex> lock(mtx);

				if (backlog.size() > 0) {
					buffer = backlog.front();
					backlog.pop_front();
				} else if (backlog.size() == 0 && closeRequested) {
					// DONE! No more work and close requested. quit thread.

					cvClose.notify_one();

					break;
				} 
			}

			if (buffer != nullptr) {
				int64_t numBytes = buffer->pos;
				indexer->bytesWritten += numBytes;
				indexer->bytesToWrite -= numBytes;

				fsOctree.write(buffer->data_char, numBytes);
				fsOctree.flush();

				indexer->bytesInMemory -= numBytes;
			} else {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(10ms);
			}
			
		}

	}).detach();
}


void Writer::closeAndWait() {
	if (closed) {
		return;
	}

	unique_lock<mutex> lock(mtx);
	if (activeBuffer != nullptr) {
		backlog.push_back(activeBuffer);
	}

	closeRequested = true;
	cvClose.wait(lock);

	fsOctree.close();

}


}




