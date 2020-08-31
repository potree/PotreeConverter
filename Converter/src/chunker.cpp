//
//#include "chunker.h"
//
//#include <mutex>
//#include <atomic>
//#include <memory>
//
//#include "laszip/laszip_api.h"
//
//#include "ConcurrentWriter.h"
//#include "logger.h"
//#include "structures.h"
//#include "unsuck/TaskPool.hpp"
//
//using std::mutex;
//using std::unique_ptr;
//
//namespace chunker {
//
//	auto numChunkerThreads = getCpuData().numProcessors;
//	auto numFlushThreads = getCpuData().numProcessors;
//
//	int maxPointsPerChunk = 5'000'000;
//	int gridSize = 128;
//	mutex mtx_attributes;
//
//	struct Point {
//		double x;
//		double y;
//		double z;
//	};
//
//	unordered_map<int, vector<vector<Point>>> buckets;
//
//	ConcurrentWriter* writer = nullptr;
//
//	struct Node {
//
//		string id = "";
//		int64_t level = 0;
//		int64_t x = 0;
//		int64_t y = 0;
//		int64_t z = 0;
//		int64_t size;
//		int64_t numPoints;
//
//		Node(string id, int numPoints) {
//			this->id = id;
//			this->numPoints = numPoints;
//		}
//	};
//
//	vector<Node> nodes;
//
//	string toNodeID(int level, int gridSize, int64_t x, int64_t y, int64_t z) {
//
//		string id = "r";
//
//		int currentGridSize = gridSize;
//		int lx = x;
//		int ly = y;
//		int lz = z;
//
//		for (int i = 0; i < level; i++) {
//
//			int index = 0;
//
//			if (lx >= currentGridSize / 2) {
//				index = index + 0b100;
//				lx = lx - currentGridSize / 2;
//			}
//
//			if (ly >= currentGridSize / 2) {
//				index = index + 0b010;
//				ly = ly - currentGridSize / 2;
//			}
//
//			if (lz >= currentGridSize / 2) {
//				index = index + 0b001;
//				lz = lz - currentGridSize / 2;
//			}
//
//			id = id + to_string(index);
//			currentGridSize = currentGridSize / 2;
//		}
//
//		return id;
//	}
//
//	template<class T>
//	double asDouble(uint8_t* data) {
//		T value = reinterpret_cast<T*>(data)[0];
//
//		return double(value);
//	}
//
//
//	// grid contains index of node in nodes
//	struct NodeLUT {
//		int64_t gridSize;
//		vector<int> grid;
//	};
//
//	//Points readLasLaz(string path, int firstPoint, int numPoints) {
//
//	//	laszip_POINTER laszip_reader;
//	//	{
//	//		laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
//	//		laszip_BOOL request_reader = 1;
//
//	//		laszip_create(&laszip_reader);
//	//		laszip_request_compatibility_mode(laszip_reader, request_reader);
//	//		laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);
//	//		laszip_seek_point(laszip_reader, firstPoint);
//	//	}
//
//	//	double coordinates[3];
//	//	for (int i = 0; i < numPoints; i++) {
//	//		int64_t pointOffset = i * bpp;
//
//	//		laszip_read_point(laszip_reader);
//	//		laszip_get_coordinates(laszip_reader, coordinates);
//
//	//	}
//
//
//	//}
//
//
//	vector<std::atomic_int32_t> countPointsInCells(vector<Source> sources, Vector3 min, Vector3 max, int64_t gridSize, State& state, Attributes& outputAttributes) {
//
//		cout << endl;
//		cout << "=======================================" << endl;
//		cout << "=== COUNTING                           " << endl;
//		cout << "=======================================" << endl;
//
//		auto tStart = now();
//
//		//Vector3 size = max - min;
//
//		vector<std::atomic_int32_t> grid(gridSize * gridSize * gridSize);
//
//		struct Task {
//			string path;
//			int64_t totalPoints = 0;
//			int64_t firstPoint;
//			int64_t firstByte;
//			int64_t numBytes;
//			int64_t numPoints;
//			int64_t bpp;
//			Vector3 scale;
//			Vector3 offset;
//			Vector3 min;
//			Vector3 max;
//		};
//
//		auto processor = [gridSize, &grid, tStart, &state, &outputAttributes](shared_ptr<Task> task) {
//			string path = task->path;
//			int64_t start = task->firstByte;
//			int64_t numBytes = task->numBytes;
//			int64_t numToRead = task->numPoints;
//			int64_t bpp = task->bpp;
//			//Vector3 scale = task->scale;
//			//Vector3 offset = task->offset;
//			Vector3 min = task->min;
//			Vector3 max = task->max;
//
//			thread_local unique_ptr<void, void(*)(void*)> buffer(nullptr, free);
//			thread_local int64_t bufferSize = -1;
//
//			{ // sanity checks
//				if (numBytes < 0) {
//					logger::ERROR("invalid malloc size: " + formatNumber(numBytes));
//				}
//			}
//
//			if (bufferSize < numBytes) {
//				buffer.reset(malloc(numBytes));
//				bufferSize = numBytes;
//			}
//
//			laszip_POINTER laszip_reader;
//			{
//				laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
//				laszip_BOOL request_reader = 1;
//
//				laszip_create(&laszip_reader);
//				laszip_request_compatibility_mode(laszip_reader, request_reader);
//				laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);
//				laszip_seek_point(laszip_reader, task->firstPoint);
//			}
//
//			double cubeSize = (max - min).max();
//			Vector3 size = { cubeSize, cubeSize, cubeSize };
//			max = min + cubeSize;
//
//			double dGridSize = double(gridSize);
//
//			double coordinates[3];
//
//			auto posScale = outputAttributes.posScale;
//			auto posOffset = outputAttributes.posOffset;
//
//			for (int i = 0; i < numToRead; i++) {
//				int64_t pointOffset = i * bpp;
//
//				laszip_read_point(laszip_reader);
//				laszip_get_coordinates(laszip_reader, coordinates);
//
//				{
//					// transfer las integer coordinates to new scale/offset/box values
//					double x = coordinates[0];
//					double y = coordinates[1];
//					double z = coordinates[2];
//
//					int32_t X = int32_t((x - posOffset.x) / posScale.x);
//					int32_t Y = int32_t((y - posOffset.y) / posScale.y);
//					int32_t Z = int32_t((z - posOffset.z) / posScale.z);
//
//					double ux = (double(X) * posScale.x + posOffset.x - min.x) / size.x;
//					double uy = (double(Y) * posScale.y + posOffset.y - min.y) / size.y;
//					double uz = (double(Z) * posScale.z + posOffset.z - min.z) / size.z;
//
//					int64_t ix = int64_t(std::min(dGridSize * ux, dGridSize - 1.0));
//					int64_t iy = int64_t(std::min(dGridSize * uy, dGridSize - 1.0));
//					int64_t iz = int64_t(std::min(dGridSize * uz, dGridSize - 1.0));
//
//					int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;
//
//					grid[index]++;
//				}
//
//			}
//
//			laszip_close_reader(laszip_reader);
//			laszip_destroy(laszip_reader);
//
//			static int64_t pointsProcessed = 0;
//			pointsProcessed += task->numPoints;
//
//			state.name = "COUNTING";
//			state.pointsProcessed = pointsProcessed;
//			state.duration = now() - tStart;
//
//			//cout << ("end: " + formatNumber(dbgCurr)) << endl;
//		};
//
//		TaskPool<Task> pool(numChunkerThreads, processor);
//
//		auto tStartTaskAssembly = now();
//
//		for (auto source : sources) {
//			//auto parallel = std::execution::par;
//			//for_each(parallel, paths.begin(), paths.end(), [&mtx, &sources](string path) {
//
//			laszip_POINTER laszip_reader;
//			laszip_header* header;
//			//laszip_point* point;
//			{
//				laszip_create(&laszip_reader);
//
//				laszip_BOOL request_reader = 1;
//				laszip_BOOL is_compressed = iEndsWith(source.path, ".laz") ? 1 : 0;
//
//				laszip_request_compatibility_mode(laszip_reader, request_reader);
//				laszip_open_reader(laszip_reader, source.path.c_str(), &is_compressed);
//				laszip_get_header_pointer(laszip_reader, &header);
//			}
//
//			int64_t bpp = header->point_data_record_length;
//			int64_t numPoints = std::max(uint64_t(header->number_of_point_records), header->extended_number_of_point_records);
//
//			int64_t pointsLeft = numPoints;
//			int64_t batchSize = 1'000'000;
//			int64_t numRead = 0;
//
//			while (pointsLeft > 0) {
//
//				int64_t numToRead;
//				if (pointsLeft < batchSize) {
//					numToRead = pointsLeft;
//					pointsLeft = 0;
//				} else {
//					numToRead = batchSize;
//					pointsLeft = pointsLeft - batchSize;
//				}
//
//				int64_t firstByte = header->offset_to_point_data + numRead * bpp;
//				int64_t numBytes = numToRead * bpp;
//
//				auto task = make_shared<Task>();
//				task->path = source.path;
//				task->totalPoints = numPoints;
//				task->firstPoint = numRead;
//				task->firstByte = firstByte;
//				task->numBytes = numBytes;
//				task->numPoints = numToRead;
//				task->bpp = header->point_data_record_length;
//				//task->scale = { header->x_scale_factor, header->y_scale_factor, header->z_scale_factor };
//				//task->offset = { header->x_offset, header->y_offset, header->z_offset };
//				task->min = min;
//				task->max = max;
//
//				pool.addTask(task);
//
//				numRead += batchSize;
//			}
//
//			laszip_close_reader(laszip_reader);
//			laszip_destroy(laszip_reader);
//		}
//
//		printElapsedTime("tStartTaskAssembly", tStartTaskAssembly);
//
//		pool.waitTillEmpty();
//		pool.close();
//
//		printElapsedTime("countPointsInCells", tStart);
//
//		double duration = now() - tStart;
//
//		cout << "finished counting in " << formatNumber(duration) << "s" << endl;
//		cout << "=======================================" << endl;
//
//		{
//			double duration = now() - tStart;
//			state.values["duration(chunking-count)"] = formatNumber(duration, 3);
//		}
//
//
//		return std::move(grid);
//	}
//
//	void doChunking(vector<Source> sources, string targetDir, Vector3 min, Vector3 max, State& state, Attributes outputAttributes) {
//		auto tStart = now();
//
//		int64_t tmp = state.pointsTotal / 20;
//		maxPointsPerChunk = std::min(tmp, int64_t(10'000'000));
//		cout << "maxPointsPerChunk: " << maxPointsPerChunk << endl;
//
//		if (state.pointsTotal < 100'000'000) {
//			gridSize = 128;
//		} else if (state.pointsTotal < 500'000'000) {
//			gridSize = 256;
//		} else {
//			gridSize = 512;
//		}
//
//		state.currentPass = 1;
//
//		{ // prepare/clean target directories
//			string dir = targetDir + "/chunks";
//			fs::create_directories(dir);
//
//			for (const auto& entry : std::filesystem::directory_iterator(dir)) {
//				std::filesystem::remove(entry);
//			}
//		}
//
//		// COUNT
//		auto grid = countPointsInCells(sources, min, max, gridSize, state, outputAttributes);
//
//		{ // DISTIRBUTE
//			auto tStartDistribute = now();
//
//			auto lut = createLUT(grid, gridSize);
//
//			state.currentPass = 2;
//			distributePoints(sources, min, max, targetDir, lut, state, outputAttributes);
//
//			{
//				double duration = now() - tStartDistribute;
//				state.values["duration(chunking-distribute)"] = formatNumber(duration, 3);
//			}
//		}
//
//
//		string metadataPath = targetDir + "/chunks/metadata.json";
//		double cubeSize = (max - min).max();
//		Vector3 size = { cubeSize, cubeSize, cubeSize };
//		max = min + cubeSize;
//
//		writeMetadata(metadataPath, min, max, outputAttributes);
//
//		double duration = now() - tStart;
//		state.values["duration(chunking-total)"] = formatNumber(duration, 3);
//	}
//
//}