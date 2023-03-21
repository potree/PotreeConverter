
#pragma once

#include <execution>

#include "structures.h"
#include "Attributes.h"



struct SamplerWeighted : public Sampler {

	// subsample a local octree from bottom up
	void sample(Node* node, Attributes attributes, double baseSpacing, 
		function<void(Node*)> onNodeCompleted,
		function<void(Node*)> onNodeDiscarded
	) {

		int64_t gridSize = 128;
		double fGridSize = 128;
		/*threadlocal auto gridbuffer = make_shared<Buffer>(4 * gridSize * gridSize * gridSize * sizeof(uint32_t));
		uint32_t* grid = gridbuffer->data_u32;*/
		thread_local vector<uint32_t> grid(4 * gridSize* gridSize* gridSize, 0);

		struct Point {
			double x;
			double y;
			double z;
			int32_t pointIndex;
			int32_t childIndex;
		};

		function<void(Node*, function<void(Node*)>)> traversePost = [&traversePost](Node* node, function<void(Node*)> callback) {
			for (auto child : node->children) {

				if (child != nullptr && !child->sampled) {
					traversePost(child.get(), callback);
				}
			}

			callback(node);
		};

		int bytesPerPoint = attributes.bytes;
		Vector3 scale = attributes.posScale;
		Vector3 offset = attributes.posOffset;

		traversePost(node, [&](Node* node) {
			node->sampled = true;

			if(node->isLeaf()) return true;

			int64_t numPoints = node->numPoints;

			// clear grid
			for(int i = 0; i < gridSize * gridSize * gridSize; i++){
				grid[i] = 0;
			}

			auto max = node->max;
			auto min = node->min;
			auto size = max - min;
			auto scale = attributes.posScale;
			auto offset = attributes.posOffset;

			struct CellIndex {
				int64_t index = -1;
				double distance = 0.0;
			};

			auto toCellIndex = [min, size, gridSize](Vector3 point) -> CellIndex {

				double nx = (point.x - min.x) / size.x;
				double ny = (point.y - min.y) / size.y;
				double nz = (point.z - min.z) / size.z;

				double lx = 2.0 * fmod(double(gridSize) * nx, 1.0) - 1.0;
				double ly = 2.0 * fmod(double(gridSize) * ny, 1.0) - 1.0;
				double lz = 2.0 * fmod(double(gridSize) * nz, 1.0) - 1.0;

				double distance = sqrt(lx * lx + ly * ly + lz * lz);

				int64_t x = double(gridSize) * nx;
				int64_t y = double(gridSize) * ny;
				int64_t z = double(gridSize) * nz;

				x = std::max(int64_t(0), std::min(x, gridSize - 1));
				y = std::max(int64_t(0), std::min(y, gridSize - 1));
				z = std::max(int64_t(0), std::min(z, gridSize - 1));

				int64_t index = x + y * gridSize + z * gridSize * gridSize;

				return { index, distance };
			};

			int numAccepted = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if(child == nullptr) continue;

				for (int i = 0; i < child->numPoints; i++) {

					int64_t pointOffset = i * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					double x = (xyz[0] * scale.x) + offset.x;
					double y = (xyz[1] * scale.y) + offset.y;
					double z = (xyz[2] * scale.z) + offset.z;

					CellIndex cellIndex = toCellIndex({ x, y, z });

					// auto& gridValue = grid[cellIndex.index];

					if(grid[4 * cellIndex.index + 3] == 0){
						numAccepted++;
					};

					grid[4 * cellIndex.index + 0] += 255;
					grid[4 * cellIndex.index + 1] += 255;
					grid[4 * cellIndex.index + 2] += 255;
					grid[4 * cellIndex.index + 3] += 1;
				}
			}


			auto nodesize = node->max - node->min;
			auto accepted = make_shared<Buffer>(numAccepted * attributes.bytes);

			int numProcessed = 0;
			for (int voxelIndex = 0; voxelIndex < gridSize * gridSize * gridSize; voxelIndex++) {

				auto count = grid[4 * voxelIndex + 3];

				if(count == 0) continue;

				int ix = voxelIndex % gridSize;
				int iy = (voxelIndex % (gridSize * gridSize)) / gridSize;
				int iz = voxelIndex / (gridSize * gridSize);
				
				double x = node->min.x + ((double(ix) + 0.5f) / fGridSize) * nodesize.x;
				double y = node->min.y + ((double(iy) + 0.5f) / fGridSize) * nodesize.y;
				double z = node->min.z + ((double(iz) + 0.5f) / fGridSize) * nodesize.z;

				int X = (x - offset.x) / scale.x;
				int Y = (y - offset.y) / scale.y;
				int Z = (z - offset.z) / scale.z;

				accepted->set<int>(X, attributes.bytes * numProcessed + 0);
				accepted->set<int>(Y, attributes.bytes * numProcessed + 4);
				accepted->set<int>(Z, attributes.bytes * numProcessed + 8);

				uint16_t R = 255;
				uint16_t G = 0;
				uint16_t B = 255;
				accepted->set<uint16_t>(R, attributes.bytes * numProcessed + 12);
				accepted->set<uint16_t>(G, attributes.bytes * numProcessed + 14);
				accepted->set<uint16_t>(B, attributes.bytes * numProcessed + 16);

				numProcessed++;
			}

			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) continue;

				onNodeCompleted(child.get());
			}

			node->points = accepted;
			node->numPoints = numAccepted;

			return true;
		});
	}

};

