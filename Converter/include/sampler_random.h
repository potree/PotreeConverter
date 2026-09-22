
#pragma once

#include <execution>
#include <random>
#include <chrono>
#include <cstring>
#include <print>

#include "structures.h"
#include "Attributes.h"
#include "VBuffer.h"
#include "VBufferPool.h"
#include "BitEdit.h"

using std::println;

struct SamplerRandom : public Sampler {

	// subsample a local octree from bottom up
	void sample(Node* node, Attributes attributes, double baseSpacing, 
		function<void(Node*)> onNodeCompleted,
		function<void(Node*)> onNodeDiscarded
	) {

		if(enableTrace) println("sample node {}", node->name);

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

		bool enableTrace = this->enableTrace;
		traversePost(node, [enableTrace, bytesPerPoint, baseSpacing, scale, offset, &onNodeCompleted, &onNodeDiscarded, attributes](Node* node) {
			node->sampled = true;
			
			// if(enableTrace) println("traversing node {}", node->name);
			if(enableTrace) logger::INFO(format("traversing node {}", node->name));

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
				double distanceSquared = 0.0;
			};

			// auto toCellIndex = [min, size, gridSize](Vector3 point) -> CellIndex {

			// 	double nx = (point.x - min.x) / size.x;
			// 	double ny = (point.y - min.y) / size.y;
			// 	double nz = (point.z - min.z) / size.z;

			// 	double lx = 2.0 * fmod(double(gridSize) * nx, 1.0) - 1.0;
			// 	double ly = 2.0 * fmod(double(gridSize) * ny, 1.0) - 1.0;
			// 	double lz = 2.0 * fmod(double(gridSize) * nz, 1.0) - 1.0;

			// 	double distance = sqrt(lx * lx + ly * ly + lz * lz);

			// 	int64_t x = double(gridSize) * nx;
			// 	int64_t y = double(gridSize) * ny;
			// 	int64_t z = double(gridSize) * nz;

			// 	x = std::max(int64_t(0), std::min(x, gridSize - 1));
			// 	y = std::max(int64_t(0), std::min(y, gridSize - 1));
			// 	z = std::max(int64_t(0), std::min(z, gridSize - 1));

			// 	int64_t index = x + y * gridSize + z * gridSize * gridSize;

			// 	return { index, distance };
			// };
			
			double fx = double(gridSize) / size.x;
			double fy = double(gridSize) / size.y;
			double fz = double(gridSize) / size.z;

			auto toCellIndex = [min, fx, fy, fz, gridSize](Vector3 point) -> CellIndex {

				// grid-space coordinates (multiply instead of divide)
				double tx = (point.x - min.x) * fx;
				double ty = (point.y - min.y) * fy;
				double tz = (point.z - min.z) * fz;

				int64_t x = int64_t(tx);
				int64_t y = int64_t(ty);
				int64_t z = int64_t(tz);

				// fractional position within the cell, mapped to [-1, 1].
				// tx - x is exactly fmod(tx, 1.0): both truncate toward zero.
				double lx = 2.0 * (tx - double(x)) - 1.0;
				double ly = 2.0 * (ty - double(y)) - 1.0;
				double lz = 2.0 * (tz - double(z)) - 1.0;

				double distanceSquared = lx * lx + ly * ly + lz * lz;

				x = std::max(int64_t(0), std::min(x, gridSize - 1));
				y = std::max(int64_t(0), std::min(y, gridSize - 1));
				z = std::max(int64_t(0), std::min(z, gridSize - 1));

				int64_t index = x + y * gridSize + z * gridSize * gridSize;

				return { index, distanceSquared };
			};
			
			if(enableTrace) logger::INFO("0000");

			bool isLeaf = node->isLeaf();
			if (isLeaf) {
				// shuffle

				u8* pointBuffer = node->points->ptr;
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
			
			if(enableTrace) logger::INFO("1000");

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
			
			if(enableTrace) logger::INFO("2000");
			
			// Needs one bit per point in all child nodes, plus some extra padding
			thread_local shared_ptr<VBuffer> acceptedChildPointsBuffer = VBuffer::create(10'000'000);
			acceptedChildPointsBuffer->commit(numPointsInChildren / 8 + 256);
			memset(acceptedChildPointsBuffer->ptr, 0, numPointsInChildren / 8 + 256);
			
			
			if(enableTrace) logger::INFO("3000");

			i64 numRejectedPerChild[8] = {0};
			int64_t numAccepted = 0;
			i64 processedPointCounter = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				if(enableTrace) logger::INFO(format("childIndex				{}", childIndex));
				if(enableTrace) logger::INFO(format("node:					{}", u64(node)));
				if(enableTrace) logger::INFO(format("node->children.size()	{}", node->children.size()));
				
				auto child = node->children[childIndex];
				
				if(enableTrace) logger::INFO("3100");

				if (child == nullptr) {
				if(enableTrace) logger::INFO("3150");
					continue;
				}
				if(enableTrace) logger::INFO("3200");

				int64_t numRejected = 0;
				
				if(enableTrace){
					logger::INFO(format("processing points of childIndex				{}", childIndex));
					logger::INFO(format("child->numPoints 								{:L}", child->numPoints));
					logger::INFO(format("child->points->ptr 							{}", u64(child->points->ptr)));
					logger::INFO(format("acceptedChildPointsBuffer->comittedCapacity	{}", acceptedChildPointsBuffer->comittedCapacity));
				}

				for (int i = 0; i < child->numPoints; i++) {
					
					if(enableTrace && i % 1000 == 0){
						logger::INFO(format("processed points: {:L}", i));
					}

					if(enableTrace) logger::INFO("3600");
					int64_t pointOffset = i * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->ptr + pointOffset);

					double x = (xyz[0] * scale.x) + offset.x;
					double y = (xyz[1] * scale.y) + offset.y;
					double z = (xyz[2] * scale.z) + offset.z;
					if(enableTrace) logger::INFO("3601");

					CellIndex cellIndex = toCellIndex({ x, y, z });

					auto& gridValue = grid[cellIndex.index];

					// static double all = sqrt(3.0);
					constexpr double threshold = (0.7 * 1.7320508075688772) * (0.7 * 1.7320508075688772);
					
					bool isAccepted;
					if (child->numPoints < 100) {
						isAccepted = true;
					} else if (cellIndex.distanceSquared < threshold && gridValue < iteration) {
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
						if(enableTrace) logger::INFO("3800");
						BitEdit::writeU32((u32*)acceptedChildPointsBuffer->ptr, processedPointCounter, 1, 1);
						if(enableTrace) logger::INFO("3801");
					}
					
					processedPointCounter++;
				}

				numRejectedPerChild[childIndex] = numRejected;
			}
			
			if(enableTrace) logger::INFO("4000");

			// auto accepted = make_shared<Buffer>(numAccepted * attributes.bytes);
			auto accepted = VBufferPool::acquire();
			accepted->commit(numAccepted * attributes.bytes);
			
			if(enableTrace) logger::INFO("4500");
			
			processedPointCounter = 0;
			i64 numAcceptedProcessed = 0;
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				
				if(enableTrace) logger::INFO(format("node->children.size(): {}", node->children.size()));
				
				auto child = node->children[childIndex];
				
				if(enableTrace){
					if(child){
						logger::INFO(format("node->children[childIndex]: {}", child->name));
					}else{
						logger::INFO(format("node->children[childIndex]: null"));
					}
					
				}

				if (child == nullptr) continue;

				auto numRejected = numRejectedPerChild[childIndex];
				i64 numRejectedCompacted = 0;

				for (int i = 0; i < child->numPoints; i++) {
					
					if(enableTrace){
						if(processedPointCounter >= acceptedChildPointsBuffer->comittedCapacity / 32){
							logger::INFO(format("error: {} >= {}", processedPointCounter, acceptedChildPointsBuffer->comittedCapacity / 32));
						}
					}
					
					bool isAccepted = BitEdit::readU32((u32*)acceptedChildPointsBuffer->ptr, processedPointCounter, 1) == 1;
					int64_t pointOffset = i * attributes.bytes;

					if (isAccepted) {
						
						accepted->memcpy(
							numAcceptedProcessed * attributes.bytes,
							child->points->ptr + pointOffset,
							attributes.bytes
						);
						
						// memcpy(
						// 	accepted->ptr + numAcceptedProcessed * attributes.bytes,
						// 	child->points->ptr + pointOffset, 
						// 	attributes.bytes
						// );
						numAcceptedProcessed++;
					} else {
						child->points->memcpy(
							numRejectedCompacted * attributes.bytes,
							child->points->ptr + pointOffset,
							attributes.bytes
						);
						// memcpy(
						// 	child->points->ptr + numRejectedCompacted * attributes.bytes,
						// 	child->points->ptr + pointOffset,
						// 	attributes.bytes
						// );
						numRejectedCompacted++;
					}
					
					processedPointCounter++;
				}
				child->points->size = numRejectedCompacted * attributes.bytes;
				
				if(enableTrace) logger::INFO(format("4600"));

				if (numRejected == 0 && child->isLeaf()) {
					if(enableTrace) logger::INFO(format("4700"));
					onNodeDiscarded(child.get());

					node->children[childIndex] = nullptr;
				} if (numRejected > 0) {
					if(enableTrace) logger::INFO(format("4800"));
					child->numPoints = numRejected;

					onNodeCompleted(child.get());
				} else if(numRejected == 0) {
					if(enableTrace) logger::INFO(format("4900"));
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
			
			if(enableTrace) logger::INFO("5000");

			node->points = accepted;
			node->numPoints = numAccepted;

			return true;
		});
	}

};

