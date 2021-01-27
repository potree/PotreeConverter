
#pragma once

#include <string>
#include <algorithm>

#include "converter_utils.h"

using std::string;

namespace ChunkRefiner {

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
		}

		auto chunks = make_shared<Chunks>(chunksToLoad, min, max);
		chunks->attributes = attributes;

		return chunks;
	}

	void refineChunk(shared_ptr<Chunk> chunk, Attributes attributes) {

		cout << "refine " << chunk->id << endl;

		vector<vector<uint8_t>> chunkParts;

		int64_t gridSize = 128;
		vector<std::atomic_int32_t> counters(gridSize * gridSize * gridSize);

		struct Task{
			int64_t start = 0;
			int64_t size = 0;
			int64_t numPoints = 0;
		};

		TaskPool<Task> pool(16, [chunk, &chunkParts, attributes, &counters, gridSize](auto task){
			vector<uint8_t> points = readBinaryFile(chunk->file, task->start, task->size);

			auto min = chunk->min;
			auto size = chunk->max - min;
			auto scale = attributes.posScale;
			auto offset = attributes.posOffset;
			auto bpp = attributes.bytes;

			auto gridIndexOf = [&points, bpp, scale, offset, min, size, gridSize](int64_t pointIndex) {

				int64_t pointOffset = pointIndex * bpp;
				int32_t* xyz = reinterpret_cast<int32_t*>(&points[0] + pointOffset);

				double x = (xyz[0] * scale.x) + offset.x;
				double y = (xyz[1] * scale.y) + offset.y;
				double z = (xyz[2] * scale.z) + offset.z;

				int64_t ix = double(gridSize) * (x - min.x) / size.x;
				int64_t iy = double(gridSize) * (y - min.y) / size.y;
				int64_t iz = double(gridSize) * (z - min.z) / size.z;

				ix = std::max(0ll, std::min(ix, gridSize - 1));
				iy = std::max(0ll, std::min(iy, gridSize - 1));
				iz = std::max(0ll, std::min(iz, gridSize - 1));

				int64_t index = mortonEncode_magicbits(iz, iy, ix);

				return index;
			};

			for(int64_t i = 0; i < task->numPoints; i++){
				auto index = gridIndexOf(i);
				counters[index]++;
			}

			chunkParts.push_back(std::move(points));
		});

		int64_t filesize = fs::file_size(chunk->file);
		int64_t numPoints = filesize / attributes.bytes;
		int64_t pointsLeft = numPoints;
		int64_t defaultBatchSize = 1'000'000;

		int64_t start = 0;
		while(pointsLeft > 0){
			int64_t batchSize = std::min(defaultBatchSize, pointsLeft);

			auto task = make_shared<Task>();
			task->numPoints = batchSize;
			task->start = start;
			task->size = batchSize * attributes.bytes;

			pool.addTask(task);

			start += batchSize * attributes.bytes;
			
			pointsLeft -= batchSize;
		}

		pool.waitTillEmpty();
		pool.close();



		// auto numPoints = points->size / bpp;

		

		

		

		// for(int64_t i = 0; i < numPoints; i++){
		// 	auto index = gridIndexOf(i);
		// 	counters[index]++;
		// }
		


	}

	void refine(string targetDir, State& state) {
		
		int64_t maxPointsPerChunk = 10'000'000;

		printElapsedTime("refine start", 0);
		
		auto chunks = getChunks(targetDir);

		auto bpp = chunks->attributes.bytes;
		int64_t maxFilesize = maxPointsPerChunk * bpp;

		vector<shared_ptr<Chunk>> tooLargeChunks;

		for (auto chunk : chunks->list) {
			auto filesize = fs::file_size(chunk->file);

			if (filesize > maxFilesize) {
				tooLargeChunks.push_back(chunk);

				cout << chunk->file << endl;
			}

		}

		for (auto chunk : tooLargeChunks) {
			refineChunk(chunk, chunks->attributes);
		}

		printElapsedTime("refine end", 0);
		


	}


}


