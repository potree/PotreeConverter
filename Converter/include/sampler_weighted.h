
#pragma once

#include <execution>
#include <format>

#include "structures.h"
#include "Attributes.h"

using std::format;


struct SamplerWeighted {

	int64_t gridSize = 256;
	double dGridSize = 256;

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
		int64_t gridSize, double dGridSize,
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

			auto childsize = child->max - child->min;

			struct Sample {
				double x;
				double y;
				double z;
				uint8_t r;
				uint8_t g;
				uint8_t b;
			};
		

			auto getSample = [&](int index) -> Sample {

				if(child->numPoints > 0){
					// Point Sample

					int64_t pointOffset = index * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					Sample sample;
					sample.x = (xyz[0] * scale.x) + offset.x;
					sample.y = (xyz[1] * scale.y) + offset.y;
					sample.z = (xyz[2] * scale.z) + offset.z;

					uint32_t R = 0;
					uint32_t G = 0;
					uint32_t B = 0;

					if(att_rgb){
						R = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 0);
						G = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 2);
						B = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 4);
					}
					sample.r = R < 256 ? R : R / 256;
					sample.g = G < 256 ? G : G / 256;
					sample.b = B < 256 ? B : B / 256;

					return sample;
				}else{
					// Voxel Sample

					Voxel voxel = child->voxels[index];

					Sample sample;
					sample.x = (double(voxel.x + 0.5) / dGridSize) * childsize.x + child->min.x;
					sample.y = (double(voxel.y + 0.5) / dGridSize) * childsize.y + child->min.y;
					sample.z = (double(voxel.z + 0.5) / dGridSize) * childsize.z + child->min.z;
					sample.r = voxel.r;
					sample.g = voxel.g;
					sample.b = voxel.b;
				
					return sample;
				}
			
			};

			//TODO: not just points, also voxels!!!
			int numSamples = std::max(child->numPoints, child->numVoxels);
			for (int i = 0; i < numSamples; i++) {

				Sample sample = getSample(i);

				double fx = dGridSize * (sample.x - min.x) / size.x;
				double fy = dGridSize * (sample.y - min.y) / size.y;
				double fz = dGridSize * (sample.z - min.z) / size.z;

				int ix = clamp(fx, 0.0, dGridSize - 1);
				int iy = clamp(fy, 0.0, dGridSize - 1);
				int iz = clamp(fz, 0.0, dGridSize - 1);

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

				// uint32_t R = 0;
				// uint32_t G = 0;
				// uint32_t B = 0;

				// if(att_rgb){
				// 	R = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 0);
				// 	G = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 2);
				// 	B = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 4);
				// }

				// R = R < 256 ? R : R / 256;
				// G = G < 256 ? G : G / 256;
				// B = B < 256 ? B : B / 256;

				grid_r[voxelIndex] += W * sample.r;
				grid_g[voxelIndex] += W * sample.g;
				grid_b[voxelIndex] += W * sample.b;
				grid_w[voxelIndex] += W;
			}
		}

	}

	void voxelizePrimitives_neighbors(
		Node* node, Attributes& attributes, 
		Vector3& scale, Vector3& offset,
		Vector3& min, Vector3& size,
		int64_t gridSize, double dGridSize,
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

			struct Sample {
				double x;
				double y;
				double z;
				uint8_t r;
				uint8_t g;
				uint8_t b;
			};
		
			auto childsize = child->max - child->min;

			auto getSample = [&](int index) -> Sample {

				if(child->numPoints > 0){
					// Point Sample

					int64_t pointOffset = index * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					Sample sample;
					sample.x = (xyz[0] * scale.x) + offset.x;
					sample.y = (xyz[1] * scale.y) + offset.y;
					sample.z = (xyz[2] * scale.z) + offset.z;

					uint32_t R = 0;
					uint32_t G = 0;
					uint32_t B = 0;

					if(att_rgb){
						R = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 0);
						G = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 2);
						B = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 4);
					}
					sample.r = R < 256 ? R : R / 256;
					sample.g = G < 256 ? G : G / 256;
					sample.b = B < 256 ? B : B / 256;

					return sample;
				}else{
					// Voxel Sample

					Voxel voxel = child->voxels[index];

					Sample sample;
					sample.x = (double(voxel.x + 0.5) / dGridSize) * childsize.x + child->min.x;
					sample.y = (double(voxel.y + 0.5) / dGridSize) * childsize.y + child->min.y;
					sample.z = (double(voxel.z + 0.5) / dGridSize) * childsize.z + child->min.z;
					sample.r = voxel.r;
					sample.g = voxel.g;
					sample.b = voxel.b;
				
					return sample;
				}
			
			};

			int numSamples = std::max(child->numPoints, child->numVoxels);
			for (int i = 0; i < numSamples; i++) {

				Sample sample = getSample(i);

				// int64_t pointOffset = i * attributes.bytes;
				// int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

				// double x = (xyz[0] * scale.x) + offset.x;
				// double y = (xyz[1] * scale.y) + offset.y;
				// double z = (xyz[2] * scale.z) + offset.z;

				// double fx = dGridSize * (x - min.x) / size.x;
				// double fy = dGridSize * (y - min.y) / size.y;
				// double fz = dGridSize * (z - min.z) / size.z;

				// int ix = clamp(fx, 0.0, dGridSize - 1);
				// int iy = clamp(fy, 0.0, dGridSize - 1);
				// int iz = clamp(fz, 0.0, dGridSize - 1);

				double fx = dGridSize * (sample.x - min.x) / size.x;
				double fy = dGridSize * (sample.y - min.y) / size.y;
				double fz = dGridSize * (sample.z - min.z) / size.z;

				int ix = clamp(fx, 0.0, dGridSize - 1);
				int iy = clamp(fy, 0.0, dGridSize - 1);
				int iz = clamp(fz, 0.0, dGridSize - 1);

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
							// uint32_t R = 0;
							// uint32_t G = 0;
							// uint32_t B = 0;

							// if(att_rgb){
							// 	R = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 0);
							// 	G = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 2);
							// 	B = child->points->get<uint16_t>(pointOffset + att_rgb_offset + 4);
							// }

							// R = R < 256 ? R : R / 256;
							// G = G < 256 ? G : G / 256;
							// B = B < 256 ? B : B / 256;

							grid_r[voxelIndex] += W * sample.r;
							grid_g[voxelIndex] += W * sample.g;
							grid_b[voxelIndex] += W * sample.b;
							grid_w[voxelIndex] += W;
						}
						
					}
				}
			}
		} 

	}

	vector<Voxel> extract(
		Node* node, Attributes& attributes, 
		Vector3& scale, Vector3& offset,
		Vector3& min, Vector3& size,
		int64_t gridSize, double dGridSize,
		uint32_t& numAccepted, Buffer* accepted,
		vector<uint32_t>& grid_r,
		vector<uint32_t>& grid_g,
		vector<uint32_t>& grid_b,
		vector<uint32_t>& grid_w
	){
		vector<Voxel> acceptedVoxels;
		acceptedVoxels.reserve(numAccepted);
		// auto acceptedPoints = make_shared<Buffer>(numAccepted * attributes.bytes);
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
				
				double x = node->min.x + ((double(ix) + 0.5f) / dGridSize) * nodesize.x;
				double y = node->min.y + ((double(iy) + 0.5f) / dGridSize) * nodesize.y;
				double z = node->min.z + ((double(iz) + 0.5f) / dGridSize) * nodesize.z;

				int X = (x - offset.x) / scale.x;
				int Y = (y - offset.y) / scale.y;
				int Z = (z - offset.z) / scale.z;

				Voxel voxel;
				voxel.x = ix;
				voxel.y = iy;
				voxel.z = iz;
				// acceptedPoints->set<int>(X, attributes.bytes * numProcessed + 0);
				// acceptedPoints->set<int>(Y, attributes.bytes * numProcessed + 4);
				// acceptedPoints->set<int>(Z, attributes.bytes * numProcessed + 8);

				uint16_t R = grid_r[voxelIndex] / W;
				uint16_t G = grid_g[voxelIndex] / W;
				uint16_t B = grid_b[voxelIndex] / W;
				// acceptedPoints->set<uint16_t>(R, attributes.bytes * numProcessed + 12);
				// acceptedPoints->set<uint16_t>(G, attributes.bytes * numProcessed + 14);
				// acceptedPoints->set<uint16_t>(B, attributes.bytes * numProcessed + 16);

				voxel.r = R < 256 ? R : R / 256;
				voxel.g = G < 256 ? G : G / 256;
				voxel.b = B < 256 ? B : B / 256;

				voxel.mortonCode = mortonEncode_magicbits(iz, iy, ix);
				// voxel.mortonCode = mortonEncode_magicbits(ix, iy, iz);

				acceptedVoxels.push_back(voxel);
				numProcessed++;
			}

			grid_r[voxelIndex] = 0;
			grid_g[voxelIndex] = 0;
			grid_b[voxelIndex] = 0;
			grid_w[voxelIndex] = 0;
		}

		return acceptedVoxels;
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
			
			node->sampled = true;

			if(node->isLeaf()){
				//node->voxels = ;
				node->numVoxels = 0;
				
				// PROTO: Create crap data for testing
				node->byteSize = 16 * node->numPoints;
				static uint64_t offset = 0;
				node->byteOffset = offset;
				offset += node->byteSize;

				onNodeCompleted(node);
			}else{


				int64_t numPoints = node->numPoints;

				auto max = node->max;
				auto min = node->min;
				auto size = max - min;
				auto scale = attributes.posScale;
				auto offset = attributes.posOffset;

				// first, project points to voxel and count; add to <accepted> if first in voxel
				auto tStartCentral = now();
				numAccepted = 0;
				voxelizePrimitives_central(
					node, attributes, scale, offset, min, size, gridSize, dGridSize,
					numAccepted, accepted.get(),
					grid_r, grid_g, grid_b, grid_w
				);

				// second, project points to neighbourhood of voxel, 
				// and only count if the adjacent voxel also holds geometry
				auto tStartAdjacent = now();
				voxelizePrimitives_neighbors(
					node, attributes, scale, offset, min, size, gridSize, dGridSize,
					numAccepted, accepted.get(),
					grid_r, grid_g, grid_b, grid_w
				);
				
				// third, use <accepted> to extract voxels
				auto tStartExtract = now();
				auto extracted = extract(
					node, attributes, scale, offset, min, size, gridSize, dGridSize,
					numAccepted, accepted.get(),
					grid_r, grid_g, grid_b, grid_w
				);

				auto tSort = now();
				sort(extracted.begin(), extracted.end(), [](Voxel& a, Voxel& b){
					return a.mortonCode < b.mortonCode;
				});

				auto tDone = now();

				printfmt("sampled {:6} central: {:2.1f} ms, adjacent: {:2.1f} ms, extract: {:2.1f} ms, sort: {:2.1f} ms \n", 
					node->name,
					(tStartAdjacent - tStartCentral) * 1000.0,
					(tStartExtract - tStartAdjacent) * 1000.0,
					(tSort - tStartExtract) * 1000.0,
					(tDone - tSort) * 1000.0
				);

				node->voxels = extracted;
				node->numVoxels = extracted.size();
				node->numPoints = 0;

				// PROTO: Create crap data for testing
				node->byteSize = 16 * node->numVoxels;
				static uint64_t byteOffset = 0;
				node->byteOffset = byteOffset ;
				byteOffset += node->byteSize;

				onNodeCompleted(node);
			}

			// for(auto child : node->children){
			// 	if(child == nullptr) continue;
			// 	onNodeCompleted(child.get());
			// }

		});

		// PROT: write to file
		// if(false)
		// localRoot->traverse([&](Node* node){

		// 	// if(node->level() > 3){
		// 	// 	return;
		// 	// }

		// 	string dir = "G:/temp/proto";

		// 	Attribute* att_rgb = attributes.get("rgb");
		// 	int att_rgb_offset = attributes.getOffset("rgb");
			
		// 	if(node->isLeaf()){
		// 		// write points

		// 		stringstream ss;

		// 		for(int i = 0; i < node->numPoints; i++){
		// 			int X = node->points->get<int32_t>(i * attributes.bytes + 0);
		// 			int Y = node->points->get<int32_t>(i * attributes.bytes + 4);
		// 			int Z = node->points->get<int32_t>(i * attributes.bytes + 8);

		// 			double x = (X * scale.x) + offset.x;
		// 			double y = (Y * scale.y) + offset.y;
		// 			double z = (Z * scale.z) + offset.z;

		// 			uint32_t R = 0;
		// 			uint32_t G = 0;
		// 			uint32_t B = 0;

		// 			if(att_rgb){
		// 				R = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 0);
		// 				G = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 2);
		// 				B = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 4);
		// 			}

		// 			ss << format("{}, {}, {}, {}, {}, {}", x, y, z, R, G, B);

		// 			if(i < node->numPoints - 1){
		// 				ss << "\n";
		// 			}
		// 		}

		// 		string path = format("{}/{}.csv", dir, node->name);
		// 		writeFile(path, ss.str());

		// 		//int bytesize = (node->numPoints * 4) / 8 + node->numPoints * 3;
		// 		int bytesize = node->numPoints * 16;
		// 		int kbsize = bytesize / 1024;
		// 		cout << format("#points: {:7}; {:4} kb \n", node->numPoints, kbsize);

		// 	}else{
		// 		// write voxels

		// 		stringstream ss;

		// 		for(int i = 0; i < node->numPoints; i++){
		// 			int X = node->points->get<int32_t>(i * attributes.bytes + 0);
		// 			int Y = node->points->get<int32_t>(i * attributes.bytes + 4);
		// 			int Z = node->points->get<int32_t>(i * attributes.bytes + 8);

		// 			double x = (X * scale.x) + offset.x;
		// 			double y = (Y * scale.y) + offset.y;
		// 			double z = (Z * scale.z) + offset.z;

		// 			uint32_t R = 0;
		// 			uint32_t G = 0;
		// 			uint32_t B = 0;

		// 			if(att_rgb){
		// 				R = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 0);
		// 				G = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 2);
		// 				B = node->points->get<uint16_t>(i * attributes.bytes + att_rgb_offset + 4);
		// 			}

		// 			ss << format("{}, {}, {}, {}, {}, {}", x, y, z, R, G, B);

		// 			if(i < node->numPoints - 1){
		// 				ss << "\n";
		// 			}
		// 		}

		// 		string path = format("{}/{}.csv", dir, node->name);
		// 		writeFile(path, ss.str());

		// 		int bytesize = (node->numPoints * 4) / 8 + node->numPoints * 3;
		// 		int kbsize = bytesize / 1024;
		// 		cout << format("#voxels: {:7}; {:4} kb \n", node->numPoints, kbsize);
		// 	}

		// });

		//cout << format("processing {} - done \n", localRoot->name);
	}

};

