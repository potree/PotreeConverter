
#pragma once

#include <execution>
#include <format>

#include "structures.h"
#include "Attributes.h"

using std::format;

struct SamplerWeighted {

	int64_t gridSize = 256;
	double fGridSize = 256;

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

		Attribute* att_rgb = attributes.get("rgb");
		int att_rgb_offset = attributes.getOffset("rgb");

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

				int ix = clamp(fx, 0.0, fGridSize - 1);
				int iy = clamp(fy, 0.0, fGridSize - 1);
				int iz = clamp(fz, 0.0, fGridSize - 1);

				int64_t voxelIndex = ix + iy * gridSize + iz * gridSize * gridSize;

				Vector3 samplePos = {
					floor(fx) + 0.5,
					floor(fy) + 0.5,
					floor(fz) + 0.5
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

				uint64_t W = clamp(100.0 * w, 1.0, 100.0);

				if(grid_w[voxelIndex] == 0){
					accepted->data_u32[numAccepted] = voxelIndex;
					numAccepted++;
				};

				uint32_t R = 0;
				uint32_t G = 0;
				uint32_t B = 0;

				if(att_rgb){
					R = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 0);
					G = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 2);
					B = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 4);
				}

				R = R < 256 ? R : R / 256;
				G = G < 256 ? G : G / 256;
				B = B < 256 ? B : B / 256;

				grid_r[voxelIndex] += W * R;
				grid_g[voxelIndex] += W * G;
				grid_b[voxelIndex] += W * B;
				grid_w[voxelIndex] += W;
			}
		}

	}

	void voxelizePrimitives_neighbors(
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

		Attribute* att_rgb = attributes.get("rgb");
		int att_rgb_offset = attributes.getOffset("rgb");

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

				int ix = clamp(fx, 0.0, fGridSize - 1);
				int iy = clamp(fy, 0.0, fGridSize - 1);
				int iz = clamp(fz, 0.0, fGridSize - 1);

				for(int ox = -1; ox <= 1; ox++)
				for(int oy = -1; oy <= 1; oy++)
				for(int oz = -1; oz <= 1; oz++)
				{

					if(ox == 0 && oy == 0 && oz == 0) continue;
					if(ix == 0 || ix == gridSize - 1) continue;
					if(iy == 0 || iy == gridSize - 1) continue;
					if(iz == 0 || iz == gridSize - 1) continue;

					uint32_t voxelIndex = (ix + ox) + gridSize * (iy + oy) + gridSize * gridSize * (iz + oz);

					uint32_t currentW = grid_w[voxelIndex];

					if(currentW == 0) continue;

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

						uint32_t currentW = grid_w[voxelIndex];

						if(currentW > 0){
							uint32_t R = 0;
							uint32_t G = 0;
							uint32_t B = 0;

							if(att_rgb){
								R = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 0);
								G = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 2);
								B = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 4);
							}

							R = R < 256 ? R : R / 256;
							G = G < 256 ? G : G / 256;
							B = B < 256 ? B : B / 256;

							grid_r[voxelIndex] += W * R;
							grid_g[voxelIndex] += W * G;
							grid_b[voxelIndex] += W * B;
							grid_w[voxelIndex] += W;
						}
						
					}
				}
			}
		} 

	}

	shared_ptr<Buffer> extract(
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
		auto acceptedPoints = make_shared<Buffer>(numAccepted * attributes.bytes);
		auto nodesize = node->max - node->min;
		int numProcessed = 0;
		int g3 = gridSize * gridSize * gridSize;

		for(int i = 0; i < numAccepted; i++) {

			uint32_t voxelIndex = accepted->data_u32[i];

			auto W = grid_w[voxelIndex];

			if(W > 0){

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

				uint16_t R = grid_r[voxelIndex] / W;
				uint16_t G = grid_g[voxelIndex] / W;
				uint16_t B = grid_b[voxelIndex] / W;
				acceptedPoints->set<uint16_t>(R, attributes.bytes * numProcessed + 12);
				acceptedPoints->set<uint16_t>(G, attributes.bytes * numProcessed + 14);
				acceptedPoints->set<uint16_t>(B, attributes.bytes * numProcessed + 16);

				numProcessed++;
			}

			grid_r[voxelIndex] = 0;
			grid_g[voxelIndex] = 0;
			grid_b[voxelIndex] = 0;
			grid_w[voxelIndex] = 0;
		}

		return acceptedPoints;
	}

	// subsample a local octree from bottom up
	void sample(Node* localRoot, Attributes attributes, double baseSpacing, 
		function<void(Node*)> onNodeCompleted,
		function<void(Node*)> onNodeDiscarded
	) {

		//cout << format("processing {} - start\n", localRoot->name);

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

			// second, project points to neighbourhood of voxel, 
			// and only count if the adjacent voxel also holds geometry
			voxelizePrimitives_neighbors(
				node, attributes, scale, offset, min, size, gridSize, fGridSize,
				numAccepted, accepted.get(),
				grid_r, grid_g, grid_b, grid_w
			);
			
			// third, use <accepted> to extract voxels
			auto extractedPoints = extract(
				node, attributes, scale, offset, min, size, gridSize, fGridSize,
				numAccepted, accepted.get(),
				grid_r, grid_g, grid_b, grid_w
			);

			node->points = extractedPoints;
			node->numPoints = numAccepted;

			return;
		});

		// PROT: write to file
		if(false)
		localRoot->traverse([&](Node* node){

			if(node->level() > 3){
				return;
			}

			string dir = "G:/temp/proto";

			Attribute* att_rgb = attributes.get("rgb");
			int att_rgb_offset = attributes.getOffset("rgb");
			
			if(node->isLeaf()){
				// write points

				stringstream ss;

				for(int i = 0; i < node->numPoints; i++){
					int X = node->points->get<int32_t>(i * attributes.bytes + 0);
					int Y = node->points->get<int32_t>(i * attributes.bytes + 4);
					int Z = node->points->get<int32_t>(i * attributes.bytes + 8);

					double x = (X * scale.x) + offset.x;
					double y = (Y * scale.y) + offset.y;
					double z = (Z * scale.z) + offset.z;

					uint32_t R = 0;
					uint32_t G = 0;
					uint32_t B = 0;

					if(att_rgb){
						R = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 0);
						G = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 2);
						B = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 4);
					}

					ss << format("{}, {}, {}, {}, {}, {}", x, y, z, R, G, B);

					if(i < node->numPoints - 1){
						ss << "\n";
					}
				}

				string path = format("{}/{}.csv", dir, node->name);
				writeFile(path, ss.str());

			}else{
				// write voxels

				stringstream ss;

				for(int i = 0; i < node->numPoints; i++){
					int X = node->points->get<int32_t>(i * attributes.bytes + 0);
					int Y = node->points->get<int32_t>(i * attributes.bytes + 4);
					int Z = node->points->get<int32_t>(i * attributes.bytes + 8);

					double x = (X * scale.x) + offset.x;
					double y = (Y * scale.y) + offset.y;
					double z = (Z * scale.z) + offset.z;

					uint32_t R = 0;
					uint32_t G = 0;
					uint32_t B = 0;

					if(att_rgb){
						R = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 0);
						G = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 2);
						B = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 4);
					}

					ss << format("{}, {}, {}, {}, {}, {}", x, y, z, R, G, B);

					if(i < node->numPoints - 1){
						ss << "\n";
					}
				}

				string path = format("{}/{}.csv", dir, node->name);
				writeFile(path, ss.str());
			}

		});

		//cout << format("processing {} - done \n", localRoot->name);
	}

};

