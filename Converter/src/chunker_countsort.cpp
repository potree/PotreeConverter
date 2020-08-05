
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>

#include "chunker_countsort.h"

#include "Attributes.h"
#include "LasLoader/LasLoader.h"
#include "converter_utils.h"
#include "unsuck/unsuck.hpp"
#include "unsuck/TaskPool.hpp"
#include "Vector3.h"
#include "ConcurrentWriter.h"

#include "json/json.hpp"

using json = nlohmann::json;

using std::cout;
using std::endl;
using std::unordered_map;
using std::thread;
using std::mutex;
using std::lock_guard;
using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;
using std::atomic_int32_t;

namespace fs = std::filesystem;



namespace chunker_countsort {

	constexpr int numChunkerThreads = 12;
	constexpr int numFlushThreads = 12;
	constexpr int maxPointsPerChunk = 5'000'000;
	constexpr int gridSize = 128;

	struct Node;

	vector<Node> nodes;

	unordered_map<int, vector<vector<Point>>> buckets;

	ConcurrentWriter* writer = nullptr;

	struct LasTypeInfo {
		AttributeType type = AttributeType::UNDEFINED;
		int numElements = 0;
	};

	LasTypeInfo lasTypeInfo(int typeID) {

		unordered_map<int, AttributeType> mapping = {
			{0, AttributeType::UNDEFINED},
			{1, AttributeType::UINT8},
			{2, AttributeType::INT8},
			{3, AttributeType::UINT16},
			{4, AttributeType::INT16},
			{5, AttributeType::UINT32},
			{6, AttributeType::INT32},
			{7, AttributeType::UINT64},
			{8, AttributeType::INT64},
			{9, AttributeType::FLOAT},
			{10, AttributeType::DOUBLE},

			{11, AttributeType::UINT8},
			{12, AttributeType::INT8},
			{13, AttributeType::UINT16},
			{14, AttributeType::INT16},
			{15, AttributeType::UINT32},
			{16, AttributeType::INT32},
			{17, AttributeType::UINT64},
			{18, AttributeType::INT64},
			{19, AttributeType::FLOAT},
			{20, AttributeType::DOUBLE},

			{21, AttributeType::UINT8},
			{22, AttributeType::INT8},
			{23, AttributeType::UINT16},
			{24, AttributeType::INT16},
			{25, AttributeType::UINT32},
			{26, AttributeType::INT32},
			{27, AttributeType::UINT64},
			{28, AttributeType::INT64},
			{29, AttributeType::FLOAT},
			{30, AttributeType::DOUBLE},
		};

		if (mapping.find(typeID) != mapping.end()) {
			
			AttributeType type = mapping[typeID];

			int numElements = 0;
			if (typeID <= 10) {
				numElements = 1;
			} else if (typeID <= 20) {
				numElements = 2;
			} else if (typeID <= 30) {
				numElements = 3;
			}

			LasTypeInfo info;
			info.type = type;
			info.numElements = numElements;

			return info;
		} else {
			cout << "ERROR: unkown extra attribute type: " << typeID << endl;
			exit(123);
		}

		
	}

	vector<Attribute> parseExtraAttributes(LasHeader& header) {

		vector<uint8_t> extraData;

		for (auto& vlr : header.vlrs) {
			if (vlr.recordID == 4) {
				extraData = vlr.data;
				break;
			}
		}
		for (auto& vlr : header.extendedVLRs) {
			if (vlr.recordID == 4) {
				extraData = vlr.data;
				break;
			}
		}

		constexpr int recordSize = 192;
		auto numExtraAttributes = extraData.size() / recordSize;
		vector<Attribute> attributes;

		for (size_t i = 0; i < numExtraAttributes; i++) {

			int offset = i * recordSize;
			uint8_t type = read<uint8_t>(extraData, offset + 2);
			uint8_t options = read<uint8_t>(extraData, offset + 3);
			char chrName[32];
			memcpy(chrName, extraData.data() + offset + 4, 32);
			string name(chrName);

			auto info = lasTypeInfo(type);
			string typeName = getAttributeTypename(info.type);
			int elementSize = getAttributeTypeSize(info.type);

			//cout << "== EXTRA ATTRIBUTE ==" << endl;
			//cout << "type: " << int(type) << endl;
			//cout << "strName: '" << name << "'" << endl;
			//cout << "typename: " << typeName << endl;
			//cout << "numElements: " << info.numElements << endl;
			//cout << "=====================" << endl;

			int size = info.numElements * elementSize;
			Attribute xyz(name, size, info.numElements, elementSize, info.type);

			attributes.push_back(xyz);
		}

		return attributes;
	}

	Attributes getAttributes(LasHeader& header) {
		auto format = header.pointDataFormat;

		vector<Attribute> list;

		Attribute xyz("position", 12, 3, 4, AttributeType::INT32);
		Attribute intensity("intensity", 2, 1, 2, AttributeType::UINT16);
		Attribute returns("returns", 1, 1, 1, AttributeType::UINT8);
		Attribute classification("classification", 1, 1, 1, AttributeType::UINT8);
		Attribute scanAngleRank("scan angle rank", 1, 1, 1, AttributeType::UINT8);
		Attribute userData("user data", 1, 1, 1, AttributeType::UINT8);
		Attribute pointSourceId("point source id", 2, 1, 2, AttributeType::UINT16);
		Attribute gpsTime("gps-time", 8, 1, 8, AttributeType::DOUBLE);
		Attribute rgb("rgb", 6, 3, 2, AttributeType::UINT16);

		if (format == 0) {
			list = {xyz, intensity, returns, classification, scanAngleRank, userData, pointSourceId};
		} else if (format == 1) {
			list = { xyz, intensity, returns, classification, scanAngleRank, userData, pointSourceId, gpsTime};
		} else if (format == 2) {
			list = {xyz, intensity, returns, classification, scanAngleRank, userData, pointSourceId, rgb};
		} else if (format == 3) {
			list = {xyz, intensity, returns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb};
		} else{
			cout << "ERROR: currently unsupported LAS format: " << format << endl;
		}

		vector<Attribute> extraAttributes = parseExtraAttributes(header);

		list.insert(list.end(), extraAttributes.begin(), extraAttributes.end());

		auto attributes = Attributes(list);
		attributes.posScale = { header.scale[0], header.scale[1], header.scale[2] };
		attributes.posOffset = { header.offset[0], header.offset[1], header.offset[2] };

		return attributes;
	}


	struct Node {
		string id = "";
		int numPoints = 0;

		Node(string id, int numPoints) {
			this->id = id;
			this->numPoints = numPoints;
		}
	};

	struct ChunkWriter {

		vector<Node> nodes;
		vector<atomic_int32_t> semaphores;


		ChunkWriter() {

		}

	};

	string toNodeID(int level, int gridSize, int x, int y, int z) {

		string id = "r";

		int currentGridSize = gridSize;
		int lx = x;
		int ly = y;
		int lz = z;

		for (int i = 0; i < level; i++) {

			int index = 0;

			if (lx >= currentGridSize / 2) {
				index = index + 0b100;
				lx = lx - currentGridSize / 2;
			}

			if (ly >= currentGridSize / 2) {
				index = index + 0b010;
				ly = ly - currentGridSize / 2;
			}

			if (lz >= currentGridSize / 2) {
				index = index + 0b001;
				lz = lz - currentGridSize / 2;
			}

			id = id + to_string(index);
			currentGridSize = currentGridSize / 2;
		}

		return id;
	}

	vector<vector<int64_t>> createSumPyramid(vector<std::atomic_int32_t>& grid, int gridSize) {

		auto tStart = now();

		int level = std::log2(gridSize);
		int currentGridSize = gridSize;
		vector<int64_t> currentGrid;
		currentGrid.reserve(grid.size());
		for (auto& value : grid) {
			currentGrid.push_back(value);
		}

		vector<vector<int64_t>> sumPyramid(level + 1);
		sumPyramid[level] = currentGrid;

		while (level > 0) {
			//cout << "=======================" << endl;
			//cout << "level: " << level << endl;
			//cout << "gridSize: " << currentGridSize << endl;

			int nextGridSize = currentGridSize / 2;

			vector<int64_t> nextGrid(nextGridSize * nextGridSize * nextGridSize);

			for (int x = 0; x < currentGridSize; x += 2) {
				for (int y = 0; y < currentGridSize; y += 2) {
					for (int z = 0; z < currentGridSize; z += 2) {

						int64_t sum = 0;

						for (int ox : {0, 1}) {
							for (int oy : {0, 1}) {
								for (int oz : {0, 1}) {
									int64_t index = (x + ox) + (y + oy) * currentGridSize + (z + oz) * currentGridSize * currentGridSize;
									int64_t value = currentGrid[index];

									sum += value;
								}
							}
						}

						int64_t nextIndex = (x / 2) + (y / 2) * nextGridSize + (z / 2) * nextGridSize * nextGridSize;
						nextGrid[nextIndex] = sum;

					}
				}
			}

			currentGrid = nextGrid;
			currentGridSize = nextGridSize;

			//cout << endl << endl;

			level--;

			sumPyramid[level] = nextGrid;
		}

		printElapsedTime("createSumPyramid", tStart);

		// TODO: check if std::move is necessary to return this by move rather than copy
		return sumPyramid;
	}

	void createNodesTraversal(vector<vector<int64_t>>& pyramid, int level, int x, int y, int z) {

		auto& grid = pyramid[level];
		int64_t gridSize = pow(2, level);
		int64_t index = x + y * gridSize + z * gridSize * gridSize;
		auto numPoints = grid[index];

		auto nodeID = toNodeID(level, gridSize, x, y, z);

		//cout << "====" << endl;
		//cout << x << " / " << y << " / " << z << endl;
		//cout << "level: " << level << ", gridSize: " << gridSize << endl;
		//cout << nodeID << endl;

		if (numPoints < maxPointsPerChunk) {


			if (numPoints > 0) {
				// create node
				//Node node;
				//node.id = toNodeID(level, gridSize, x, y, z);
				//node.numPoints = numPoints;

				string id = toNodeID(level, gridSize, x, y, z);;

				//nodes.push_back(node);
				nodes.emplace_back(id, numPoints);
			}

		} else {
			for (int i = 0; i < 8; i++) {
				int dx = (i & 0b100) >> 2;
				int dy = (i & 0b010) >> 1;
				int dz = (i & 0b001) >> 0;

				int childX = 2 * x + dx;
				int childY = 2 * y + dy;
				int childZ = 2 * z + dz;

				createNodesTraversal(pyramid, level + 1, childX, childY, childZ);

			}
		}
	}

	void createNodes(vector<vector<int64_t>>& pyramid) {

		auto tStart = now();

		createNodesTraversal(pyramid, 0, 0, 0, 0);

		printElapsedTime("createNodes", tStart);
	}

	void addNodeReferencesToGrid(Node& node, int64_t nodeIndex, vector<int>& grid, int64_t gridSize) {

		int64_t startX = 0;
		int64_t startY = 0;
		int64_t startZ = 0;

		int64_t maxLevel = node.id.size() - 1;
		int64_t stepSize = gridSize / 2;

		for (int64_t level = 1; level <= maxLevel; level++) {
			int64_t index = node.id.at(level) - '0';

			startX += stepSize * ((index & 0b100) >> 2);
			startY += stepSize * ((index & 0b010) >> 1);
			startZ += stepSize * ((index & 0b001) >> 0);

			stepSize = stepSize / 2;
		}

		int gridLevel = log2(gridSize);
		int nodeLevel = node.id.size() - 1;
		int coverSize = pow(2, (gridLevel - nodeLevel));

		for (int x = startX; x < startX + coverSize; x++) {
			for (int y = startY; y < startY + coverSize; y++) {
				for (int z = startZ; z < startZ + coverSize; z++) {

					int64_t index = x + y * gridSize + z * gridSize * gridSize;
					grid[index] = nodeIndex;

				}
			}
		}

	}

	// grid contains index of node in nodes
	struct NodeLUT {
		int gridSize;
		vector<int> grid;
	};

	NodeLUT createLUT(int gridSize) {

		auto tStart = now();

		NodeLUT lut;

		vector<int> grid(gridSize * gridSize * gridSize, -1);

		for (int64_t i = 0; i < nodes.size(); i++) {
			Node& node = nodes[i];
			addNodeReferencesToGrid(node, i, grid, gridSize);
		}

		lut.gridSize = gridSize;
		lut.grid = grid;

		printElapsedTime("createLUT", tStart);

		return lut;
	}

	NodeLUT createLUT(vector<std::atomic_int32_t>& grid, int gridSize) {

		auto tStart = now();

		int maxLevel = std::log2(gridSize);

		auto pyramid = createSumPyramid(grid, gridSize);

		createNodes(pyramid);

		auto lut = createLUT(gridSize);

		printElapsedTime("createLUT2", tStart);

		return lut;
	}

	vector<std::atomic_int32_t> countPointsInCells(string path, LasHeader header, int64_t gridSize) {

		auto tStart = now();

		int64_t filesize = fs::file_size(path);
		int64_t bpp = header.pointDataRecordLength;
		int64_t numPoints = header.numPoints;

		int64_t pointsLeft = numPoints;
		int64_t batchSize = 1'000'000;
		int64_t numRead = 0;

		vector<thread> threads;

		vector<std::atomic_int32_t> grid(gridSize * gridSize * gridSize);

		struct Task{
			string path;
			int64_t firstByte;
			int64_t numBytes;
			int64_t numPoints;
			int64_t bpp;
			LasHeader header;
		};

		auto processor = [gridSize, &grid](shared_ptr<Task> task){
			string path = task->path;
			auto& header = task->header;
			int64_t start = task->firstByte;
			int64_t numBytes = task->numBytes;
			int64_t numToRead = task->numPoints;
			int64_t bpp = header.pointDataRecordLength;

			//thread_local void* buffer = nullptr;
			//thread_local int64_t bufferSize = -1;

			thread_local unique_ptr<void, void(*)(void*)> buffer(nullptr, free);
			thread_local int64_t bufferSize = -1;

			if (bufferSize < numBytes){
				//free(buffer.get());

				buffer.reset(malloc(numBytes));
				bufferSize = numBytes;
			}

			uint8_t* data = reinterpret_cast<uint8_t*>(buffer.get());

			readBinaryFile(path, start, numBytes, buffer.get());

			//auto data = readBinaryFile(path, start, numBytes);

			Vector3 scale = { header.scale[0], header.scale[1], header.scale[2] };
			Vector3 offset = { header.offset[0], header.offset[1], header.offset[2] };
			Vector3 min = { header.min[0], header.min[1], header.min[2] };
			Vector3 max = { header.max[0], header.max[1], header.max[2] };
			double cubeSize = (max - min).max();
			Vector3 size = { cubeSize, cubeSize, cubeSize };
			max = min + cubeSize;

			double dGridSize = gridSize;

			for (int i = 0; i < numToRead; i++) {
				int64_t pointOffset = i * bpp;

				int32_t* xyz = reinterpret_cast<int32_t*>(data + pointOffset);

				auto x = xyz[0];
				auto y = xyz[1];
				auto z = xyz[2];

				double ux = (xyz[0] * scale.x + offset.x - min.x) / size.x;
				double uy = (xyz[1] * scale.y + offset.y - min.y) / size.y;
				double uz = (xyz[2] * scale.z + offset.z - min.z) / size.z;

				int64_t ix = std::min(dGridSize * ux, dGridSize - 1.0);
				int64_t iy = std::min(dGridSize * uy, dGridSize - 1.0);
				int64_t iz = std::min(dGridSize * uz, dGridSize - 1.0);

				int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

				grid[index]++;
			}
		};

		TaskPool<Task> pool(numChunkerThreads, processor);

		while (pointsLeft > 0) {

			int64_t numToRead;
			if (pointsLeft < batchSize) {
				numToRead = pointsLeft;
				pointsLeft = 0;
			} else {
				numToRead = batchSize;
				pointsLeft = pointsLeft - batchSize;
			}

			int64_t firstByte = header.offsetToPointData + numRead * bpp;
			int64_t numBytes = numToRead * bpp;

			auto task = make_shared<Task>();
			task->path = path;
			task->firstByte = firstByte;
			task->numBytes = numBytes;
			task->numPoints = numToRead;
			task->header = header;
			
			pool.addTask(task);

			numRead += batchSize;
			//cout << result.size() << endl;
		}

		pool.waitTillEmpty();
		pool.close();

		printElapsedTime("countPointsInCells", tStart);


		return std::move(grid);
	}

	void addBuckets(string targetDir, vector<shared_ptr<Buffer>>& newBuckets) {
		//static mutex mtx_buckets;

		//lock_guard<mutex> lock(mtx_buckets);
		
		//for (auto it : newBuckets) {
		//	buckets[it.first].push_back(it.second);
		//}

		//for (auto it : newBuckets) {
		for(int nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++){

			if (newBuckets[nodeIndex]->size == 0) {
				continue;
			}

			auto& node = nodes[nodeIndex];
			string path = targetDir + "/chunks/" + node.id + ".bin";
			auto buffer = newBuckets[nodeIndex];

			//int64_t bytes = points.size() * sizeof(Point);
			//auto writeBuffer = make_shared<WriteBuffer>(bytes);
			//memcpy(writeBuffer->buffer, points.data(), bytes);
			writer->write(path, buffer);

		}
	}

	void distributePoints(string path, string targetDir, LasHeader& header, NodeLUT& lut, State& state) {

		auto tStart = now();

		writer = new ConcurrentWriter(numFlushThreads, state);

		int64_t filesize = fs::file_size(path);
		int64_t bpp = header.pointDataRecordLength;
		int64_t numPoints = header.numPoints;
		Attributes attributes = getAttributes(header);

		int64_t pointsLeft = numPoints;
		int64_t maxBatchSize = 1'000'000;
		int64_t numRead = 0;

		printElapsedTime("distributePoints0", tStart);

		vector<std::atomic_int32_t> counters(nodes.size());

		vector<thread> threads;

		struct Task {
			string path;
			LasHeader header;
			int64_t maxBatchSize;
			int64_t batchSize;
			int64_t startByte;
			int64_t numBytes;
			NodeLUT* lut;
			int64_t bpp;
		};

		mutex mtx_push_point;

		printElapsedTime("distributePoints1", tStart);

		auto processor = [&mtx_push_point, &counters, targetDir](shared_ptr<Task> task) {

			auto tStart = now();

			auto path = task->path;
			auto start = task->startByte;
			auto numBytes = task->numBytes;
			auto header = task->header;
			auto batchSize = task->batchSize;
			auto* lut = task->lut;
			auto bpp = task->bpp;

			auto gridSize = lut->gridSize;
			auto& grid = lut->grid;


			thread_local unique_ptr<void, void(*)(void*)> buffer(nullptr, free);
			thread_local int64_t bufferSize = -1;

			if (bufferSize < numBytes) {
				//free(buffer.get());

				buffer.reset(malloc(numBytes));
				bufferSize = numBytes;
			}

			uint8_t* data = reinterpret_cast<uint8_t*>(buffer.get());

			writer->waitUntilMemoryBelow(2'000);
			readBinaryFile(path, start, numBytes, buffer.get());

			Vector3 scale = { header.scale[0], header.scale[1], header.scale[2] };
			Vector3 offset = { header.offset[0], header.offset[1], header.offset[2] };
			Vector3 min = { header.min[0], header.min[1], header.min[2] };
			Vector3 max = { header.max[0], header.max[1], header.max[2] };
			double cubeSize = (max - min).max();
			Vector3 size = { cubeSize, cubeSize, cubeSize };
			max = min + cubeSize;

			int offsetRGB = -1;
			if (task->header.pointDataFormat == 2) {
				offsetRGB = 20;
			} else if (task->header.pointDataFormat == 3) {
				offsetRGB = 28;
			}

			double dGridSize = gridSize;
			
			// COUNT POINTS PER BUCKET
			vector<int64_t> counts(nodes.size(), 0);
			for (int64_t i = 0; i < batchSize; i++) {
				int64_t pointOffset = i * bpp;

				int32_t* xyz = reinterpret_cast<int32_t*>(&data[0] + pointOffset);

				auto x = xyz[0];
				auto y = xyz[1];
				auto z = xyz[2];

				double ux = (xyz[0] * scale.x + offset.x - min.x) / size.x;
				double uy = (xyz[1] * scale.y + offset.y - min.y) / size.y;
				double uz = (xyz[2] * scale.z + offset.z - min.z) / size.z;

				int64_t ix = std::min(dGridSize * ux, dGridSize - 1.0);
				int64_t iy = std::min(dGridSize * uy, dGridSize - 1.0);
				int64_t iz = std::min(dGridSize * uz, dGridSize - 1.0);

				int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

				auto nodeIndex = grid[index];
				counts[nodeIndex]++;
			}

			// ALLOCATE BUCKETS
			vector<shared_ptr<Buffer>> buckets(nodes.size(), nullptr);
			for (int i = 0; i < nodes.size(); i++) {
				int64_t numPoints = counts[i];
				int64_t bytes = numPoints * bpp;
				buckets[i] = make_shared<Buffer>(bytes);
			}
			
			// ADD POINTS TO BUCKETS
			shared_ptr<Buffer> previousBucket = nullptr;
			int64_t previousNodeIndex = -1;
			for (int64_t i = 0; i < batchSize; i++) {
				int64_t pointOffset = i * bpp;

				int32_t* xyz = reinterpret_cast<int32_t*>(&data[0] + pointOffset);

				auto x = xyz[0];
				auto y = xyz[1];
				auto z = xyz[2];

				double ux = (xyz[0] * scale.x + offset.x - min.x) / size.x;
				double uy = (xyz[1] * scale.y + offset.y - min.y) / size.y;
				double uz = (xyz[2] * scale.z + offset.z - min.z) / size.z;

				int64_t ix = std::min(dGridSize * ux, dGridSize - 1.0);
				int64_t iy = std::min(dGridSize * uy, dGridSize - 1.0);
				int64_t iz = std::min(dGridSize * uz, dGridSize - 1.0);

				int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

				auto nodeIndex = grid[index];
				auto& node = nodes[nodeIndex];

				double dx = (xyz[0] * scale.x + offset.x);
				double dy = (xyz[1] * scale.y + offset.y);
				double dz = (xyz[2] * scale.z + offset.z);

				uint8_t r = 0;
				uint8_t g = 0;
				uint8_t b = 0;
				uint8_t a = 255;

				if (offsetRGB >= 0) {
					uint16_t* rgb = reinterpret_cast<uint16_t*>(&data[0] + pointOffset + offsetRGB);

					r = rgb[0] > 255 ? rgb[0] / 256 : rgb[0];
					g = rgb[1] > 255 ? rgb[1] / 256 : rgb[1];
					b = rgb[2] > 255 ? rgb[2] / 256 : rgb[2];
				}

				Point point(dx, dy, dz, r, g, b, a);

				if (nodeIndex == previousNodeIndex) {
					//previousBucket->push_back(point);
					previousBucket->write(&data[0] + pointOffset, bpp);
				} else {
					previousBucket = buckets[nodeIndex];
					previousNodeIndex = nodeIndex;
					//previousBucket->push_back(point);
					previousBucket->write(&data[0] + pointOffset, bpp);
				}



				
			}
			//printElapsedTime("distribute task", tStart);

			auto tAddBuckets = now();
			addBuckets(targetDir, buckets);
			//printElapsedTime("add buckets", tAddBuckets);

		};

		TaskPool<Task> pool(12, processor);

		while (pointsLeft > 0) {

			int64_t numToRead;
			if (pointsLeft < maxBatchSize) {
				numToRead = pointsLeft;
				pointsLeft = 0;
			} else {
				numToRead = maxBatchSize;
				pointsLeft = pointsLeft - maxBatchSize;
			}

			int64_t start = header.offsetToPointData + numRead * bpp;
			int64_t numBytes = numToRead * bpp;

			auto task = make_shared<Task>();
			task->maxBatchSize = maxBatchSize;
			task->batchSize = numToRead;
			task->bpp = bpp;
			task->header = header;
			task->lut = &lut;
			task->numBytes = numBytes;
			task->startByte = start;
			task->path = path;

			pool.addTask(task);

			numRead += numToRead;
			//cout << result.size() << endl;
		}

		printElapsedTime("distributePoints2", tStart);
		//pool.waitTillEmpty();
		printElapsedTime("distributePoints3", tStart);
		pool.close();

		printElapsedTime("distributePoints4", tStart);
		writer->join();

		delete writer;

		printElapsedTime("distributePoints", tStart);
	}

	void writeMetadata(string path, Vector3 min, Vector3 max, Attributes attributes) {
		json js;

		js["min"] = { min.x, min.y, min.z };
		js["max"] = { max.x, max.y, max.z };

		js["attributes"] = {};
		for (auto attribute : attributes.list) {

			json jsAttribute;
			jsAttribute["name"] = attribute.name;
			jsAttribute["size"] = attribute.size;
			jsAttribute["numElements"] = attribute.numElements;
			jsAttribute["elementSize"] = attribute.elementSize;
			jsAttribute["description"] = attribute.description;
			jsAttribute["type"] = getAttributeTypename(attribute.type);

			js["attributes"].push_back(jsAttribute);
		}

		js["scale"] = vector<double>({
			attributes.posScale.x, 
			attributes.posScale.y, 
			attributes.posScale.z});

		js["offset"] = vector<double>({
			attributes.posOffset.x,
			attributes.posOffset.y,
			attributes.posOffset.z });

		string content = js.dump(4);

		writeFile(path, content);
	}

	void doChunking(string path, string targetDir, State& state) {

		auto tStart = now();
		//auto result = readBinaryFile(path);

		{ // prepare/clean target directories
			string dir = targetDir + "/chunks";
			fs::create_directories(dir);

			for (const auto& entry : std::filesystem::directory_iterator(dir)) {
				std::filesystem::remove(entry);
			}
		}

		auto header = loadHeader(path);

		auto grid = countPointsInCells(path, header, gridSize);

		auto lut = createLUT(grid, gridSize);

		distributePoints(path, targetDir, header, lut, state);

		string metadataPath = targetDir + "/chunks/metadata.json";
		Vector3 min = { header.min[0], header.min[1], header.min[2] };
		Vector3 max = { header.max[0], header.max[1], header.max[2] };
		double cubeSize = (max - min).max();
		Vector3 size = { cubeSize, cubeSize, cubeSize };
		max = min + cubeSize;
		auto attributes = getAttributes(header);
		writeMetadata(metadataPath, min, max, attributes);

		auto duration = now() - tStart;
		int64_t bpp = header.pointDataRecordLength;
		int64_t numPoints = header.numPoints;


		double GB = 1024ll * 1024ll * 1024ll;
		double gbSec = (double(numPoints * bpp) / duration) / GB;
		int pointsSec = (double(numPoints) / duration) / 1'000'000.0;

		cout << "numPoints: " << numPoints << endl;
		cout << "duration: " << duration << endl;
		cout << "GB/sec: " << gbSec << endl;
		cout << "points/sec: " << pointsSec << endl;
	}


}
