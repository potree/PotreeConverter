
#pragma once

#include <execution>
#include <random>
#include <chrono>
#include <cstring>

#include "structures.h"
#include "Attributes.h"
#include "VBuffer.h"
#include "BitEdit.h"



struct SamplerRandom : public Sampler {

	// subsample a local octree from bottom up
	void sample(Node* node, Attributes attributes, double baseSpacing, 
		function<void(Node*)> onNodeCompleted,
		function<void(Node*)> onNodeDiscarded
	) {


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

		traversePost(node, [bytesPerPoint, baseSpacing, scale, offset, &onNodeCompleted, &onNodeDiscarded, attributes](Node* node) {
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
				// shuffle

				u8* pointBuffer = node->points->data_u8;
				i64 numPoints = node->numPoints;
				i64 bytesPerPoint = attributes.bytes;
				
				// Fisher-Yates shuffle of pointBuffer, in place.
				// - Each point in pointBuffer has <bytesPerPoint> size.
				// - There are <numPoints> points in pointBuffer that need to be shuffled.
				if (numPoints > 1) {
					unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
					thread_local std::mt19937_64 rng(seed);

					thread_local vector<u8> tmp;
					tmp.resize(bytesPerPoint);

					for (i64 i = numPoints - 1; i > 0; i--) {
						std::uniform_int_distribution<i64> dist(0, i);
						i64 j = dist(rng);

						if (i == j) continue;

						u8* a = pointBuffer + i * bytesPerPoint;
						u8* b = pointBuffer + j * bytesPerPoint;

						memcpy(tmp.data(), a, bytesPerPoint);
						memcpy(a, b, bytesPerPoint);
						memcpy(b, tmp.data(), bytesPerPoint);
					}
				}



				return false;
			}

			// =================================================================
			// SAMPLING
			// =================================================================
			//
			// first, check for each point whether it's accepted or rejected
			// save result in an array with one element for each point
			
			
			
			// u32* acceptedChildPoints[8] = {0};
			
			i64 numPointsInChildren = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];
				
				if(child){
					numPointsInChildren += child->numPoints;
				}
			}
			
			// Needs one bit per point in all child nodes, plus some extra padding
			thread_local VBuffer acceptedChildPointsBuffer = VBuffer::create(10'000'000);
			acceptedChildPointsBuffer.commit(numPointsInChildren / 8 + 256);
			memset(acceptedChildPointsBuffer.ptr, 0, numPointsInChildren / 8 + 256);

			i64 numRejectedPerChild[8] = {0};
			int64_t numAccepted = 0;
			i64 processedPointCounter = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {

					continue;
				}

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

					if(isAccepted){
						BitEdit::writeU32((u32*)acceptedChildPointsBuffer.ptr, processedPointCounter, 1, 1);
					}
					
					processedPointCounter++;
				}

				numRejectedPerChild[childIndex] = numRejected;
			}

			auto accepted = make_shared<Buffer>(numAccepted * attributes.bytes);
			processedPointCounter = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) continue;

				auto numRejected = numRejectedPerChild[childIndex];
				i64 numRejectedCompacted = 0;

				for (int i = 0; i < child->numPoints; i++) {
					bool isAccepted = BitEdit::readU32((u32*)acceptedChildPointsBuffer.ptr, processedPointCounter, 1) == 1;
					int64_t pointOffset = i * attributes.bytes;

					if (isAccepted) {
						accepted->write(child->points->data_u8 + pointOffset, attributes.bytes);
					} else {
						memcpy(
							child->points->data_u8 + numRejectedCompacted * attributes.bytes,
							child->points->data_u8 + pointOffset,
							attributes.bytes
						);
						numRejectedCompacted++;
					}
					
					processedPointCounter++;
				}
				child->points->size = numRejectedCompacted * attributes.bytes;

				if (numRejected == 0 && child->isLeaf()) {
					onNodeDiscarded(child.get());

					node->children[childIndex] = nullptr;
				} if (numRejected > 0) {
					child->numPoints = numRejected;

					onNodeCompleted(child.get());
				} else if(numRejected == 0) {
					// the parent has taken all points from this child, 
					// so make this child an empty inner node.
					// Otherwise, the hierarchy file will claim that 
					// this node has points but because it doesn't have any,
					// decompressing the nonexistent point buffer fails
					// https://github.com/potree/potree/issues/1125
					child->points = nullptr;
					child->numPoints = 0;
					onNodeCompleted(child.get());
				}
			}

			node->points = accepted;
			node->numPoints = numAccepted;

			return true;
		});
	}

};

