
#pragma once

#include <execution>
#include <format>

#include "structures.h"
#include "Attributes.h"

using std::format;

struct SamplerWeighted {

	struct CellIndex {
		int64_t index = -1;
		double distance = 0.0;
	};

	struct Point {
		double x;
		double y;
		double z;
		int32_t pointIndex;
		int32_t childIndex;
	};

	template<typename T>
	T clamp(T value, T min, T max){
		if(value < min) return min;
		if(value > max) return max;

		return value;
	}

	void voxelizePrimitives_central(
		Node* node, Attributes& attributes, 
		Vector3& scale, Vector3& offset,
		Vector3& min, Vector3& size,
		int64_t gridSize, double fGridSize,
		uint32_t& numAccepted, Buffer* accepted,
		vector<uint32_t>& grid_r,
		vector<uint32_t>& grid_g,
		vector<uint32_t>& grid_b,
		vector<uint32_t>& grid_w
	){

		for (int childIndex = 0; childIndex < 8; childIndex++) {
			auto child = node->children[childIndex];

			if(child == nullptr) continue;

			for (int i = 0; i < child->numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;
				int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

				double x = (xyz[0] * scale.x) + offset.x;
				double y = (xyz[1] * scale.y) + offset.y;
				double z = (xyz[2] * scale.z) + offset.z;

				double nx = (x - min.x) / size.x;
				double ny = (y - min.y) / size.y;
				double nz = (z - min.z) / size.z;

				int64_t ix = clamp(fGridSize * nx, 0.0, fGridSize - 1.0);
				int64_t iy = clamp(fGridSize * ny, 0.0, fGridSize - 1.0);
				int64_t iz = clamp(fGridSize * nz, 0.0, fGridSize - 1.0);

				int64_t voxelIndex = ix + iy * gridSize + iz * gridSize * gridSize;

				if(grid_w[voxelIndex] == 0){
					accepted->data_u32[numAccepted] = voxelIndex;
					numAccepted++;
				};

				grid_r[voxelIndex] += 255;
				grid_g[voxelIndex] += 255;
				grid_b[voxelIndex] += 255;
				grid_w[voxelIndex] += 1;
			}
		}

	}

	// subsample a local octree from bottom up
	void sample(Node* localRoot, Attributes attributes, double baseSpacing, 
		function<void(Node*)> onNodeCompleted,
		function<void(Node*)> onNodeDiscarded
	) {

		cout << format("processing {} - start\n", localRoot->name);

		int64_t gridSize = 128;
		double fGridSize = 128;
		vector<uint32_t> grid_r(gridSize * gridSize * gridSize, 0);
		vector<uint32_t> grid_g(gridSize * gridSize * gridSize, 0);
		vector<uint32_t> grid_b(gridSize * gridSize * gridSize, 0);
		vector<uint32_t> grid_w(gridSize * gridSize * gridSize, 0);
		
		// stores uint32_t voxelIndex values
		auto accepted = make_shared<Buffer>(500'000 * sizeof(uint32_t));
		uint32_t numAccepted = 0;

		int bytesPerPoint = attributes.bytes;
		Vector3 scale = attributes.posScale;
		Vector3 offset = attributes.posOffset;

		localRoot->traversePost([&](Node* node) {

			if(node->sampled) return;
			if(node->isLeaf()) return;

			node->sampled = true;

			int64_t numPoints = node->numPoints;

			auto max = node->max;
			auto min = node->min;
			auto size = max - min;
			auto scale = attributes.posScale;
			auto offset = attributes.posOffset;

			// first, project points to voxel and count; add to <accepted> if first in voxel
			numAccepted = 0;
			voxelizePrimitives_central(
				node, attributes, scale, offset, min, size, gridSize, fGridSize,
				numAccepted, accepted.get(),
				grid_r, grid_g, grid_b, grid_w
			);
			// for (int childIndex = 0; childIndex < 8; childIndex++) {
			// 	auto child = node->children[childIndex];

			// 	if(child == nullptr) continue;

			// 	for (int i = 0; i < child->numPoints; i++) {

			// 		int64_t pointOffset = i * attributes.bytes;
			// 		int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

			// 		double x = (xyz[0] * scale.x) + offset.x;
			// 		double y = (xyz[1] * scale.y) + offset.y;
			// 		double z = (xyz[2] * scale.z) + offset.z;

			// 		uint32_t voxelIndex = toVoxelIndex({ x, y, z });

			// 		if(grid_w[voxelIndex] == 0){
			// 			accepted->data_u32[numAccepted] = voxelIndex;
			// 			numAccepted++;
			// 		};

			// 		grid_r[voxelIndex] += 255;
			// 		grid_g[voxelIndex] += 255;
			// 		grid_b[voxelIndex] += 255;
			// 		grid_w[voxelIndex] += 1;
			// 	}
			//}


			// second, project points to neighbourhood of voxel, 
			// and only count if the adjacent voxel also holds geometry
			for (int childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if(child == nullptr) continue;

				for (int i = 0; i < child->numPoints; i++) {

					int64_t pointOffset = i * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					double x = (xyz[0] * scale.x) + offset.x;
					double y = (xyz[1] * scale.y) + offset.y;
					double z = (xyz[2] * scale.z) + offset.z;

					double fx = fGridSize * (x - min.x) / size.x;
					double fy = fGridSize * (y - min.y) / size.y;
					double fz = fGridSize * (z - min.z) / size.z;

					for(double oz : {-1.0, 0.0, 1.0})
					for(double oy : {-1.0, 0.0, 1.0})
					for(double ox : {-1.0, 0.0, 1.0})
					{

						Vector3 samplePos = {
							floor(fx + ox) + 0.5,
							floor(fy + oy) + 0.5,
							floor(fz + oz) + 0.5
						};

						double dx = (fx - samplePos.x);
						double dy = (fy - samplePos.y);
						double dz = (fz - samplePos.z);
						double ll = (dx * dx + dy * dy + dz * dz);
						double w = 0.0;

						double l = sqrt(ll);

						if(ll < 1.0){
							// exponential filter
							// w = __expf(-ll * 0.5f);
							// w = clamp(w, 0.0f, 1.0f);
							
							// linear filter
							w = 1.0 - l;
						}else{
							w = 0.0;
						}

						if(w > 0.0){

							uint64_t W = clamp(100.0 * w, 1.0, 100.0);

							uint32_t ix = clamp(samplePos.x, 0.0, fGridSize - 1.0);
							uint32_t iy = clamp(samplePos.y, 0.0, fGridSize - 1.0);
							uint32_t iz = clamp(samplePos.z, 0.0, fGridSize - 1.0);

							uint32_t voxelIndex = ix + gridSize * iy + gridSize * gridSize * iz;

							bool isCenter = ox == 0.0f && oy == 0.0f && oz == 0.0f;
							bool isNeighbor = !isCenter;
							if(isNeighbor){

								uint32_t currentW = grid_w[voxelIndex];

								if(currentW > 0){
									// TODO: actual colors
									uint64_t R = W * 255;
									uint64_t G = W * 255;
									uint64_t B = W * 255;
									grid_r[voxelIndex] += R;
									grid_g[voxelIndex] += G;
									grid_b[voxelIndex] += B;
									grid_w[voxelIndex] += W;
								}
							}
						}
					}
				}
			} // end second

			// third, use <accepted> to extract voxels
			auto acceptedPoints = make_shared<Buffer>(numAccepted * attributes.bytes);
			auto nodesize = node->max - node->min;
			int numProcessed = 0;
			int g3 = gridSize * gridSize * gridSize;

			for(int i = 0; i < numAccepted; i++) {

				uint32_t voxelIndex = accepted->data_u32[i];

				auto count = grid_w[voxelIndex];

				if(count > 0){

					int ix = voxelIndex % gridSize;
					int iy = (voxelIndex % (gridSize * gridSize)) / gridSize;
					int iz = voxelIndex / (gridSize * gridSize);
					
					double x = node->min.x + ((double(ix) + 0.5f) / fGridSize) * nodesize.x;
					double y = node->min.y + ((double(iy) + 0.5f) / fGridSize) * nodesize.y;
					double z = node->min.z + ((double(iz) + 0.5f) / fGridSize) * nodesize.z;

					int X = (x - offset.x) / scale.x;
					int Y = (y - offset.y) / scale.y;
					int Z = (z - offset.z) / scale.z;

					acceptedPoints->set<int>(X, attributes.bytes * numProcessed + 0);
					acceptedPoints->set<int>(Y, attributes.bytes * numProcessed + 4);
					acceptedPoints->set<int>(Z, attributes.bytes * numProcessed + 8);

					uint16_t R = 255;
					uint16_t G = 0;
					uint16_t B = 255;
					acceptedPoints->set<uint16_t>(R, attributes.bytes * numProcessed + 12);
					acceptedPoints->set<uint16_t>(G, attributes.bytes * numProcessed + 14);
					acceptedPoints->set<uint16_t>(B, attributes.bytes * numProcessed + 16);

					numProcessed++;

					if(numProcessed > numAccepted){
						cout << "error...\n";
						int a = 10;
					}

				}

				grid_r[voxelIndex] = 0;
				grid_g[voxelIndex] = 0;
				grid_b[voxelIndex] = 0;
				grid_w[voxelIndex] = 0;
			}

			node->points = accepted;
			node->numPoints = numAccepted;

			return;
		});

		cout << format("processing {} - done \n", localRoot->name);
	}

};

