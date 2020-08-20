
#pragma once

#include <string>
#include <filesystem>
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <functional>
#include <memory>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "json/json.hpp"

#include "Attributes.h"
#include "converter_utils.h"
#include "Vector3.h"
#include "unsuck/unsuck.hpp"
#include "unsuck/TaskPool.hpp"

using json = nlohmann::json;

using std::atomic_int64_t;
using std::atomic_int64_t;
using std::deque;
using std::string;
using std::unordered_map;
using std::function;
using std::shared_ptr;
using std::make_shared;
using std::fstream;
using std::mutex;

namespace fs = std::filesystem;

namespace indexer_random {

	inline int numSampleThreads() {
		return getCpuData().numProcessors;
	}

	constexpr int maxPointsPerChunk = 10'000;

	struct Hierarchy {
		int64_t stepSize = 0;
		vector<uint8_t> buffer;
		int64_t firstChunkSize = 0;
	};

	struct Chunk {
		Vector3 min;
		Vector3 max;

		string file;
		string id;
	};

	struct Chunks {
		vector<shared_ptr<Chunk>> list;
		Vector3 min;
		Vector3 max;
		Attributes attributes;

		Chunks(vector<shared_ptr<Chunk>> list, Vector3 min, Vector3 max) {
			this->list = list;
			this->min = min;
			this->max = max;
		}

	};

	shared_ptr<Chunks> getChunks(string pathIn);

	struct Node {

		vector<shared_ptr<Node>> children;

		string name;
		shared_ptr<Buffer> points;
		Vector3 min;
		Vector3 max;

		int64_t indexStart = 0;

		int64_t byteOffset = 0;
		int64_t byteSize = 0;
		int64_t numPoints = 0;

		bool sampled = false;
		bool alreadyFlushed = false;

		Node() {

		}

		Node(string name, Vector3 min, Vector3 max);

		int64_t level() {
			return name.size() - 1;
		}

		void addDescendant(shared_ptr<Node> descendant);

		void traverse(function<void(Node*)> callback);

		void traversePost(function<void(Node*)> callback);

		bool isLeaf() {
			
			for (auto child : children) {
				if (child != nullptr) {
					return false;
				}
			}


			return true;
		}
	};

	struct Indexer;

	struct Writer {

		Indexer* indexer = nullptr;
		int64_t capacity = 16 * 1024 * 1024;

		// copy node data here first
		shared_ptr<Buffer> activeBuffer = nullptr;

		// backlog of buffers that reached capacity and are ready to be written to disk
		deque<shared_ptr<Buffer>> backlog;

		bool closeRequested = false;
		bool closed = false;
		std::condition_variable cvClose;

		fstream fsOctree;

		//thread tWrite;

		mutex mtx;

		Writer(Indexer* indexer);

		void writeAndUnload(Node* node);

		void launchWriterThread();

		void closeAndWait();

	};

	//struct FlushTask {
	//	Indexer* indexer;
	//	shared_ptr<Node> node;

	//	FlushTask(Indexer* indexer, shared_ptr<Node> node) {
	//		this->indexer = indexer;
	//		this->node = node;
	//	}
	//};

	//void flushProcessor(shared_ptr<FlushTask> task);

	struct Indexer{

		string targetDir = "";

		Attributes attributes;
		shared_ptr<Node> root;

		shared_ptr<Writer> writer;

		vector<shared_ptr<Node>> detachedParts;

		//fstream fsOctree;
		//mutex mtx_fsOctree;
		atomic_int64_t byteOffset = 0;

		double scale = 0.001;
		double spacing = 1.0;

		atomic_int64_t dbg = 0;

		//shared_ptr<TaskPool<FlushTask>> flushPool;
		atomic_int64_t bytesInMemory = 0;
		atomic_int64_t bytesToWrite = 0;
		atomic_int64_t bytesWritten = 0;

		Indexer(string targetDir) {

			this->targetDir = targetDir;

			//string octreePath = targetDir + "/octree.bin";
			//fsOctree.open(octreePath, ios::out | ios::binary);

			//flushPool = make_shared< TaskPool<FlushTask>>(numFlushThreads, flushProcessor);

			writer = make_shared<Writer>(this);
		}

		~Indexer() {
			//fsOctree.close();
		}

		void waitUntilMemoryBelow(int maxMegabytes) {
			using namespace std::chrono_literals;

			while (true) {
				auto memoryData = getMemoryData();
				auto usedMemoryMB = memoryData.virtual_usedByProcess / (1024 * 1024);
				// bytesInMemory

				if (usedMemoryMB > maxMegabytes) {
					std::this_thread::sleep_for(10ms);
				} else {
					break;
				}
			}
		}

		void doSampling(shared_ptr<Node> node, int64_t dbgShouldWriteBytes);

		void flushAndUnload(shared_ptr<Node> node);

		string createMetadata(Hierarchy hierarchy) {

			auto min = root->min;
			auto max = root->max;

			json js;


			js["name"] = "abc";
			js["boundingBox"]["min"] = { min.x, min.y, min.z };
			js["boundingBox"]["max"] = { max.x, max.y, max.z };
			js["projection"] = "";
			js["description"] = "";
			js["points"] = 123456;
			//js["scale"] = 0.001;
			js["spacing"] = spacing;

			js["scale"] = vector<double>{
				attributes.posScale.x,
				attributes.posScale.y,
				attributes.posScale.z
			};

			js["offset"] = vector<double>{
				attributes.posOffset.x,
				attributes.posOffset.y,
				attributes.posOffset.z
			};

			js["hierarchy"] = {
				{"stepSize", hierarchy.stepSize},
				{"firstChunkSize", hierarchy.firstChunkSize}
			};

			//js["firstHierarchyChunkSize"] = hierarchy.firstHierarchyChunkSize;

			json jsAttributes;
			for (auto attribute : attributes.list) {
				json jsAttribute;

				jsAttribute["name"] = attribute.name;
				jsAttribute["description"] = attribute.description;
				jsAttribute["size"] = attribute.size;
				jsAttribute["numElements"] = attribute.numElements;
				jsAttribute["elementSize"] = attribute.elementSize;
				jsAttribute["type"] = getAttributeTypename(attribute.type);

				jsAttributes.push_back(jsAttribute);
			}

			js["attributes"] = jsAttributes;

			//json jsAttributePosition;
			//jsAttributePosition["name"] = "position";
			//jsAttributePosition["type"] = "int32";
			//jsAttributePosition["scale"] = "0.001";
			//jsAttributePosition["offset"] = "3.1415";

			//json jsAttributeColor;
			//jsAttributeColor["name"] = "color";
			//jsAttributeColor["type"] = "uint8";

			//js["attributes"] = { jsAttributePosition, jsAttributeColor };


			string str = js.dump(4);

			return str;
		}

		string createDebugHierarchy() {
			function<json(Node*)> traverse = [&traverse](Node* node) -> json {

				vector<json> jsChildren;
				for (auto child : node->children) {
					if (child == nullptr) {
						continue;
					}

					json jsChild = traverse(child.get());
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
			js["hierarchy"] = traverse(root.get());

			string str = js.dump(4);


			return str;
		}

		

		//enum HNODE_TYPE {
		//	NORMAL = 0,	 // root or inner node
		//	LEAF = 1,    // 
		//	PROXY = 2,   // pointer to the next hierarchy chunk
		//};

		//struct HNode {
		//	string name = "";

		//	uint8_t type = 0;
		//	uint8_t childMask = 0;
		//	uint32_t numPoints = 0;
		//	uint64_t byteOffset = 0;
		//	uint64_t byteSize = 0;
		//};

		struct HierarchyChunk {
			string name = "";
			vector<Node*> nodes;
		};

		HierarchyChunk gatherChunk(Node* start, int levels) {
			// create vector containing start node and all descendants up to and including levels deeper
			// e.g. start 0 and levels 5 -> all nodes from level 0 to inclusive 5.

			int64_t startLevel = start->name.size() - 1;

			HierarchyChunk chunk;
			chunk.name = start->name;

			vector<Node*> stack = { start };
			while (!stack.empty()) {
				Node* node = stack.back();
				stack.pop_back();

				chunk.nodes.push_back(node);

				int64_t childLevel = node->name.size();
				if (childLevel <= startLevel + levels) {

					for (auto child : node->children) {
						if (child == nullptr) {
							continue;
						}

						stack.push_back(child.get());
					}

				}
			}

			return chunk;
		}

		vector<HierarchyChunk> createHierarchyChunks(Node* root, int hierarchyStepSize) {

			vector<HierarchyChunk> hierarchyChunks;
			vector<Node*> stack = { root };
			while (!stack.empty()) {
				Node* chunkRoot = stack.back();
				stack.pop_back();

				auto chunk = gatherChunk(chunkRoot, hierarchyStepSize);

				for (auto node : chunk.nodes) {
					bool isProxy = node->level() == chunkRoot->level() + hierarchyStepSize;

					if (isProxy) {
						stack.push_back(node);
					}

				}

				hierarchyChunks.push_back(chunk);
			}

			//for (auto& [chunkName, chunkNodes] : hierarchyChunks) {
			//	sort(chunkNodes.begin(), chunkNodes.end(), [](auto& a, auto& b) {
			//		if (a.name.size() != b.name.size()) {
			//			return a.name.size() < b.name.size();
			//		} else {
			//			return a.name < b.name;
			//		}
			//	});
			//}

			return hierarchyChunks;
		}

		void sortBreadthFirst(vector<Node*>& nodes) {
			sort(nodes.begin(), nodes.end(), [](Node* a, Node* b) {
				if (a->name.size() != b->name.size()) {
					return a->name.size() < b->name.size();
				} else {
					return a->name < b->name;
				}
			});
		}

		uint8_t childMaskOf(Node* node) {
			uint8_t mask = 0;

			for (int i = 0; i < 8; i++) {
				auto child = node->children[i];

				if (child != nullptr) {
					mask = mask | (1 << i);
				}
			}

			return mask;
		}

		Hierarchy createHierarchy(string path) {

			constexpr int hierarchyStepSize = 4;
			constexpr int bytesPerNode = 1 + 1 + 4 + 8 + 8;
			auto chunkSize = [](HierarchyChunk& chunk) {
				return chunk.nodes.size() * bytesPerNode;
			};

			auto chunks = createHierarchyChunks(root.get(), hierarchyStepSize);

			unordered_map<string, int> chunkPointers;
			vector<int64_t> chunkByteOffsets(chunks.size(), 0);
			int64_t hierarchyBufferSize = 0;

			for (size_t i = 0; i < chunks.size(); i++) {
				auto& chunk = chunks[i];
				chunkPointers[chunk.name] = i;

				sortBreadthFirst(chunk.nodes);

				if (i >= 1) {
					chunkByteOffsets[i] = chunkByteOffsets[i - 1] + chunkSize(chunks[i-1]);
				}

				hierarchyBufferSize += chunkSize(chunk);
			}

			vector<uint8_t> hierarchyBuffer(hierarchyBufferSize);

			enum TYPE {
				NORMAL = 0,
				LEAF = 1,
				PROXY = 2,
			};

			int offset = 0;
			for (int i = 0; i < chunks.size(); i++) {
				auto& chunk = chunks[i];
				auto chunkLevel = chunk.name.size() - 1;

				for (auto node : chunk.nodes) {
					bool isProxy = node->level() == chunkLevel + hierarchyStepSize;

					uint8_t childMask = childMaskOf(node);
					uint64_t targetOffset = 0;
					uint64_t targetSize = 0;
					uint32_t numPoints = uint32_t(node->numPoints);
					uint8_t type = node->isLeaf() ? TYPE::LEAF : TYPE::NORMAL;

					if (isProxy) {
						int targetChunkIndex = chunkPointers[node->name];
						auto targetChunk = chunks[targetChunkIndex];

						type = TYPE::PROXY;
						targetOffset = chunkByteOffsets[targetChunkIndex];
						targetSize = chunkSize(targetChunk);
					} else {
						targetOffset = node->byteOffset;
						targetSize = node->byteSize;
					}

					memcpy(hierarchyBuffer.data() + offset + 0, &type, 1);
					memcpy(hierarchyBuffer.data() + offset + 1, &childMask, 1);
					memcpy(hierarchyBuffer.data() + offset + 2, &numPoints, 4);
					memcpy(hierarchyBuffer.data() + offset + 6, &targetOffset, 8);
					memcpy(hierarchyBuffer.data() + offset + 14, &targetSize, 8);

					offset += bytesPerNode;
				}

			}

			Hierarchy hierarchy;
			hierarchy.stepSize = hierarchyStepSize;
			hierarchy.buffer = hierarchyBuffer;
			hierarchy.firstChunkSize = chunks[0].nodes.size() * bytesPerNode;

			return hierarchy;

		}
	};

	
	void buildHierarchy(Indexer* indexer, Node* node, shared_ptr<Buffer> points, int numPoints);

	class punct_facet : public std::numpunct<char> {
	protected:
		char do_decimal_point() const { return '.'; };
		char do_thousands_sep() const { return '\''; };
		string do_grouping() const { return "\3"; }
	};

	static void doIndexing(string targetDir, State& state, Options& options) {

		cout << endl;
		cout << "=======================================" << endl;
		cout << "=== INDEXING                           " << endl;
		cout << "=======================================" << endl;

		auto tStart = now();

		state.name = "INDEXING";
		state.currentPass = 3;
		state.pointsProcessed = 0;
		state.bytesProcessed = 0;
		state.duration = 0;

		auto chunks = getChunks(targetDir);
		auto attributes = chunks->attributes;

		Indexer indexer(targetDir);
		indexer.attributes = attributes;
		indexer.root = make_shared<Node>("r", chunks->min, chunks->max);

		struct Task {
			shared_ptr<Chunk> chunk;

			Task(shared_ptr<Chunk> chunk) {
				this->chunk = chunk;
			}
		};

		int64_t totalPoints = 0;
		int64_t totalBytes = 0;
		for (auto chunk : chunks->list) {
			auto filesize = fs::file_size(chunk->file);
			totalPoints += filesize / attributes.bytes;
			totalBytes += filesize;
		}

		int64_t pointsProcessed = 0;
		double lastReport = now();

		atomic_int64_t activeThreads = 0;
		mutex mtx_nodes;
		vector<shared_ptr<Node>> nodes;
		TaskPool<Task> pool(numSampleThreads(), [&state, &options, &activeThreads, tStart, &lastReport, &totalPoints, totalBytes, &pointsProcessed, chunks, &indexer, &nodes, &mtx_nodes](auto task) {
			auto chunk = task->chunk;
			auto chunkRoot = make_shared<Node>(chunk->id, chunk->min, chunk->max);
			auto attributes = chunks->attributes;
			int bpp = attributes.bytes;


			//indexer.waitUntilMemoryBelow(5'000);
			activeThreads++;


			auto filesize = fs::file_size(chunk->file);
			indexer.bytesInMemory += filesize;
			auto pointBuffer = readBinaryFile(chunk->file);

			if (!options.hasFlag("keep-chunks")) {
				fs::remove(chunk->file);
			}

			int64_t numPoints = pointBuffer->size / bpp;

			buildHierarchy(&indexer, chunkRoot.get(), pointBuffer, numPoints);

			indexer.doSampling(chunkRoot, pointBuffer->size);

			// add chunk root, provided it isn't the root.
			if (chunkRoot->name.size() > 1) {
				indexer.root->addDescendant(chunkRoot);
			}

			lock_guard<mutex> lock(mtx_nodes);

			pointsProcessed = pointsProcessed + numPoints;
			double progress = double(pointsProcessed) / double(totalPoints);


			if (now() - lastReport > 1.0) {

				double duration = now() - tStart;
				double throughputP = (pointsProcessed / duration) / 1'000'000;
				double throughputMB = (double(indexer.bytesWritten) / duration) / (1024 * 1024);

				auto ram = getMemoryData();
				auto CPU = getCpuData();
				double GB = 1024.0 * 1024.0 * 1024.0;

				int totalProgress = (66.6 + 0.3 * progress * 100.0);
				string strProgress = formatNumber(totalProgress) + "%";
				string strTime = formatNumber(now()) + "s";
				string strDuration = formatNumber(duration) + "s";
				string strThroughput = formatNumber(throughputP, 1) + "MPs (" + formatNumber(throughputMB) + "MBs)";
				string strRAM = formatNumber(double(ram.virtual_usedByProcess) / GB, 1) 
					+ "GB (highest " + formatNumber(double(ram.virtual_usedByProcess_max) / GB, 1) + "GB)";
				string strCPU = formatNumber(CPU.usage) + "%";

				state.pointsProcessed = pointsProcessed;
				state.duration = duration;

				// stringstream ss;
				// ss << "[" << strProgress << ", " << strTime << "], "
				// 	<< "[INDEXING: " << strProgress << ", duration: " << strDuration << ", throughput: " << strThroughput << "]"
				// 	<< "[RAM: " << strRAM << ", CPU: " << strCPU << "]" << endl;
				

				// cout << ss.str();

				lastReport = now();
			}

			nodes.push_back(chunkRoot);

			activeThreads--;
		});

		for (auto chunk : chunks->list) {

			// if (chunk->id != "r0776767") {
			// 	continue;
			// }

			auto task = make_shared<Task>(chunk);
			pool.addTask(task);
		}

		pool.waitTillEmpty();
		pool.close();

		indexer.spacing = (indexer.root->max - indexer.root->min).x / 128.0;
		
		if (chunks->list.size() == 1) {
			auto node = nodes[0];

			indexer.root = node;
		} else {
			indexer.doSampling(indexer.root, 0);
		}

		//child->numPoints = child->points.size() + child->store.size();
		indexer.flushAndUnload(indexer.root);

		printElapsedTime("sampling", tStart);


		indexer.writer->closeAndWait();
		//indexer.flushPool->waitTillEmpty();
		//indexer.flushPool->close();

		printElapsedTime("flushing", tStart);

		//vector<thread> threads;
		//indexer.root->traverse([&threads](Node* node) {
		//	auto level = node->name.size() - 1;
		//	
		//	cout << string(level, ' ') << node->name << ": " << node->points.size() << endl;

		//	string path = "C:/temp/test/converter/debug/" + node->name + ".las";

		//	threads.emplace_back([path, node]() {
		//		writeLAS(path, node->points);
		//	});
		//	
		//});

		//for (auto& t : threads) {
		//	t.join();
		//}

		string hierarchyPath = targetDir + "/hierarchy.bin";
		Hierarchy hierarchy = indexer.createHierarchy(hierarchyPath);
		writeBinaryFile(hierarchyPath, hierarchy.buffer);

		string metadataPath = targetDir + "/metadata.json";
		string metadata = indexer.createMetadata(hierarchy);
		writeFile(metadataPath, metadata);

		//cout << "flush duration: " << double(indexer.dbg) / 1'000'000 << "s" << endl;

		

		//{
		//	// create a las file stub with header and hierarchy data.
		//	// octree can then be merged with windows command "type file1 file2 > file3"

		//	int32_t metadataJsonSize = metadata.size();
		//	int32_t hierarchyDataSize = hierarchyData.size();

		//	fstream fs;
		//	fs.open("C:/temp/test/converter/metadata.las.part", ios::out | ios::binary);

		//	Buffer header(375);
		//	memset(header.data, 0, header.size);

		//	header.data_char[0] = 'L';
		//	header.data_char[1] = 'A';
		//	header.data_char[2] = 'S';
		//	header.data_char[3] = 'F';

		//	header.data_u8[24] = 1;
		//	header.data_u8[25] = 4;

		//	// header size
		//	header.set<uint16_t>(header.size, 94);

		//	// offset to point data
		//	int offsetToPointData = header.size + 4 + metadataJsonSize + 4 + hierarchyData.size();
		//	header.set<uint32_t>(offsetToPointData, 96);

		//	// format
		//	header.set<uint8_t>(3, 104);

		//	// point data record length
		//	header.set<uint16_t>(attributes.bytes, 105);

		//	header.set<double>(attributes.posScale.x, 131);
		//	header.set<double>(attributes.posScale.y, 139);
		//	header.set<double>(attributes.posScale.z, 147);

		//	header.set<double>(attributes.posOffset.x, 155);
		//	header.set<double>(attributes.posOffset.y, 163);
		//	header.set<double>(attributes.posOffset.z, 171);

		//	auto min = indexer.root->min;
		//	header.set<double>(min.x, 187);
		//	header.set<double>(min.y, 203);
		//	header.set<double>(min.z, 219);

		//	auto max = indexer.root->max;
		//	header.set<double>(max.x, 179);
		//	header.set<double>(max.y, 195);
		//	header.set<double>(max.z, 211);

		//	int64_t numPoints = 0;
		//	for (auto chunk : chunks->list) {
		//		int64_t filesize = fs::file_size(chunk->file);
		//		
		//		numPoints += filesize / attributes.bytes;
		//	}
		//	header.set<uint64_t>(numPoints, 247);

		//	fs.write(header.data_char, header.size);

		//	fs.write(reinterpret_cast<char*>(&metadataJsonSize), 4);
		//	fs.write(reinterpret_cast<char*>(&hierarchyDataSize), 4);

		//	//auto cstr = metadata.c_str();
		//	fs.write(reinterpret_cast<char*>(metadata.data()), metadata.size());
		//	fs.write(reinterpret_cast<char*>(hierarchyData.data()), hierarchyData.size());

		//	fs.close();
		//}

		printElapsedTime("metadata & hierarchy", tStart);

		double duration = now() - tStart;
		state.values["duration(indexing)"] = formatNumber(duration, 3);


	}


}