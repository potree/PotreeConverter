
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>

#include "chunker_countsort_laszip.h"

#include "Attributes.h"
#include "converter_utils.h"
#include "unsuck/unsuck.hpp"
#include "unsuck/TaskPool.hpp"
#include "Vector3.h"
#include "ConcurrentWriter.h"

#include "json/json.hpp"
#include "laszip/laszip_api.h"
#include "LasLoader/LasLoader.h"
#include "PotreeConverter.h"
#include "logger.h"

#include "robinhood_map/robin_hood.h"

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



namespace chunker_sparse { 

	void run(vector<Source> sources, string targetDir, Vector3 min, Vector3 max, State& state, Attributes outputAttributes) {

		auto tStart = now();

		for (auto source : sources) {

			string path = source.path;

			laszip_POINTER laszip_reader;
			laszip_header* header;
			laszip_point* point;

			{
				laszip_BOOL is_compressed;
				laszip_BOOL request_reader = 1;

				laszip_create(&laszip_reader);
				laszip_request_compatibility_mode(laszip_reader, request_reader);
				laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);
				laszip_get_header_pointer(laszip_reader, &header);
				laszip_get_point_pointer(laszip_reader, &point);
				// laszip_seek_point(laszip_reader, task->firstPoint);
			}

			int64_t numPoints = std::max(uint64_t(header->number_of_point_records), header->extended_number_of_point_records);
			double xyz[3];

			int gridSize = 1024;
			float fGridSize = float(gridSize);
			robin_hood::unordered_map<int32_t, int32_t> grid;
			grid.reserve(10'000'000);

			//int gridSize = 512;
			//float fGridSize = float(gridSize);
			//unordered_map<int32_t, int32_t> grid;
			//grid.reserve(10'000'000);

			//int gridSize = 512;
			//float fGridSize = float(gridSize);
			// vector<int64_t> grid(gridSize * gridSize * gridSize, 0);
			//vector<std::atomic_int32_t> grid(gridSize * gridSize * gridSize);

			double size[3] = {
				header->max_x - header->min_x,
				header->max_y - header->min_y,
				header->max_z - header->min_z
			};

			int64_t prevKey = 0;
			int64_t prevCount = 0;

			for (int64_t i = 0; i < numPoints; i++) {
				laszip_read_point(laszip_reader);
				laszip_get_coordinates(laszip_reader, xyz);

				int64_t gx = std::min(int(fGridSize * (xyz[0] - header->min_x) / size[0]), gridSize - 1);
				int64_t gy = std::min(int(fGridSize * (xyz[1] - header->min_y) / size[1]), gridSize - 1);
				int64_t gz = std::min(int(fGridSize * (xyz[2] - header->min_z) / size[2]), gridSize - 1);
				// int64_t key = (gx << 40) | (gy << 20) | (gz << 0);
				int32_t key = gx + gy * gridSize + gz * gridSize * gridSize;

				if(key == prevKey){
					prevCount++;
				}else{
					grid[prevKey] += prevCount;

					prevKey = key;
					prevCount = 1;
				}


				//grid[key]++;
			}

			grid[prevKey] += prevCount;

			cout << "grid size: " << grid.size() << endl;

		}

		double duration = now() - tStart;
		cout << "duration: " << formatNumber(duration, 1) << "s" << endl;
		state.values["duration(chunking-total)"] = formatNumber(duration, 3);

	}


}
