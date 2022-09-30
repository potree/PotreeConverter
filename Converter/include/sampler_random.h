
#pragma once

#include <execution>

#include "structures.h"
#include "Attributes.h"



struct SamplerRandom : public Sampler {

	// subsample a local octree from bottom up
	void sample(shared_ptr<Node> node, Attributes attributes, double baseSpacing, function<void(Node*)> onNodeCompleted) {

		if(node->name == "r066"){
			int a = 10;
		}

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

		traversePost(node.get(), [bytesPerPoint, baseSpacing, scale, offset, &onNodeCompleted, attributes](Node* node) {
			node->sampled = true;

			int64_t numPoints = node->numPoints;

			int64_t gridSize = 128;
			thread_local vector<int64_t> grid(gridSize* gridSize* gridSize, -1);
			thread_local int64_t iteration = 0;
			iteration++;

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

			bool isLeaf = node->isLeaf();
			if (isLeaf) {
				// shuffle?

				//
				// a not particularly efficient approach to shuffling:
				// 

				vector<int> indices(node->numPoints);
				for (int i = 0; i < node->numPoints; i++) {
					indices[i] = i;
				}

				unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

				shuffle(indices.begin(), indices.end(), std::default_random_engine(seed));

				auto buffer = make_shared<Buffer>(node->points->size);

				for (int i = 0; i < node->numPoints; i++) {

					int64_t sourceOffset = i * attributes.bytes;
					int64_t targetOffset = indices[i] * attributes.bytes;

					memcpy(buffer->data_u8 + targetOffset, node->points->data_u8 + sourceOffset, attributes.bytes);

				}

				node->points = buffer;


				return false;
			}

			// =================================================================
			// SAMPLING
			// =================================================================
			//
			// first, check for each point whether it's accepted or rejected
			// save result in an array with one element for each point

			vector<vector<int8_t>> acceptedChildPointFlags;
			vector<int64_t> numRejectedPerChild;
			int64_t numAccepted = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {
					acceptedChildPointFlags.push_back({});
					numRejectedPerChild.push_back({});

					continue;
				}

				vector<int8_t> acceptedFlags(child->numPoints, 0);
				int64_t numRejected = 0;

				for (int i = 0; i < child->numPoints; i++) {

					int64_t pointOffset = i * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					double x = (xyz[0] * scale.x) + offset.x;
					double y = (xyz[1] * scale.y) + offset.y;
					double z = (xyz[2] * scale.z) + offset.z;

					CellIndex cellIndex = toCellIndex({ x, y, z });

					auto& gridValue = grid[cellIndex.index];

					static double all = sqrt(3.0);

					bool isAccepted;
					if (child->numPoints < 100) {
						isAccepted = true;
					} else if (cellIndex.distance < 0.7 * all && gridValue < iteration) {
						isAccepted = true;
					} else {
						isAccepted = false;
					}

					if (isAccepted) {
						gridValue = iteration;
						numAccepted++;
					} else {
						numRejected++;
					}

					acceptedFlags[i] = isAccepted ? 1 : 0;
				}

				acceptedChildPointFlags.push_back(acceptedFlags);
				numRejectedPerChild.push_back(numRejected);
			}

			auto accepted = make_shared<Buffer>(numAccepted * attributes.bytes);
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {
					continue;
				}

				auto numRejected = numRejectedPerChild[childIndex];
				auto& acceptedFlags = acceptedChildPointFlags[childIndex];
				auto rejected = make_shared<Buffer>(numRejected * attributes.bytes);

				for (int i = 0; i < child->numPoints; i++) {
					auto isAccepted = acceptedFlags[i];
					int64_t pointOffset = i * attributes.bytes;

					if (isAccepted) {
						accepted->write(child->points->data_u8 + pointOffset, attributes.bytes);
					} else {
						rejected->write(child->points->data_u8 + pointOffset, attributes.bytes);
					}
				}

				if (numRejected == 0) {
					//if(node->name == "r0660"){
					//	int a = 10;
					//}

					node->children[childIndex] = nullptr;
				} if (numRejected > 0) {
					child->points = rejected;
					child->numPoints = numRejected;

					onNodeCompleted(child.get());
				}
			}

			node->points = accepted;
			node->numPoints = numAccepted;

			return true;
		});
	}

};

