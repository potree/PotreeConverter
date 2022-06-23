
#pragma once

#include <execution>

#include "structures.h"
#include "Attributes.h"
#include "PotreeConverter.h"


struct SamplerVoxels : public Sampler {

	static constexpr int gridSize = 256;
	static constexpr float fGridSize = 256;

	// subsample a local octree from bottom up
	void sample(shared_ptr<Node> node, Attributes attributes, double baseSpacing, function<void(Node*)> onNodeCompleted) {

		struct Point {
			double x;
			double y;
			double z;
			int r = 0;
			int g = 0;
			int b = 0;
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

		int64_t bytesPerPoint = attributes.bytes;
		Vector3 scale = attributes.posScale;
		Vector3 offset = attributes.posOffset;

		traversePost(node.get(), [bytesPerPoint, baseSpacing, scale, offset, &onNodeCompleted, &attributes](Node* node) {
			node->sampled = true;

			int64_t numPoints = node->numPoints;

			auto max = node->max;
			auto min = node->min;
			auto size = max - min;
			auto scale = attributes.posScale;
			auto offset = attributes.posOffset;

			bool isLeaf = node->isLeaf();

			if (isLeaf) {
				return false;
			}

			Attribute* aRGB = attributes.get("rgb");
			int rgbOffset = 0;
			if(aRGB == nullptr){
				aRGB = attributes.get("rgba");
			}else if(aRGB == nullptr){
				aRGB = attributes.get("RGB");
			}else if(aRGB == nullptr){
				aRGB = attributes.get("RGBA");
			}
			if(aRGB){
				rgbOffset = attributes.getOffset(aRGB->name);
			}

			auto getColor = [aRGB, &attributes, rgbOffset](Node* node, int pointIndex, int& r, int& g, int&b){

				int64_t pointOffset = pointIndex * attributes.bytes;

				if(aRGB){
					// int32_t* rgb = reinterpret_cast<int32_t*>(node->points->data_u8 + pointOffset + aRGB->offset);
					if(aRGB->elementSize == 1){
						r = node->points->get<uint8_t>(pointOffset + rgbOffset + 0);
						g = node->points->get<uint8_t>(pointOffset + rgbOffset + 1);
						b = node->points->get<uint8_t>(pointOffset + rgbOffset + 2);
					}else if(aRGB->elementSize == 2){
						r = node->points->get<uint16_t>(pointOffset + rgbOffset + 0);
						g = node->points->get<uint16_t>(pointOffset + rgbOffset + 1);
						b = node->points->get<uint16_t>(pointOffset + rgbOffset + 2);

						r = r > 255 ? r / 256 : r;
						g = g > 255 ? g / 256 : g;
						b = b > 255 ? b / 256 : b;
					}

				}else{
					r = 0;
					g = 0;
					b = 0;
				}

			};

			vector<Point> childPoints;
			vector<Voxel> childVoxels;

			// create list of points and voxels from children
			for (int64_t childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {
					continue;
				}

				int childX = (childIndex >> 2) & 1;
				int childY = (childIndex >> 1) & 1;
				int childZ = (childIndex >> 0) & 1;

				for (int64_t i = 0; i < child->numPoints; i++) {
					int64_t pointOffset = i * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					double x = (xyz[0] * scale.x) + offset.x;
					double y = (xyz[1] * scale.y) + offset.y;
					double z = (xyz[2] * scale.z) + offset.z;

					int r, g, b;
					getColor(child.get(), i, r, g, b);

					Point point = { x, y, z, r, g, b, i, childIndex };

					childPoints.push_back(point);
				}

				for (int64_t i = 0; i < child->voxels.size(); i++) {
					Voxel childVoxel = child->voxels[i];
					Voxel voxel;

					voxel.x = (childVoxel.x + gridSize * childX) / 2;
					voxel.y = (childVoxel.y + gridSize * childY) / 2;
					voxel.z = (childVoxel.z + gridSize * childZ) / 2;
					voxel.r = childVoxel.r;
					voxel.g = childVoxel.g;
					voxel.b = childVoxel.b;
					voxel.w = childVoxel.w;

					childVoxels.push_back(voxel);
				}
			}

			// sample points and voxels from children
			vector<double> grid(gridSize * gridSize * gridSize * 4, 0.0);
			// std::sort(childPoints.begin(), childPoints.end(), [](Point& a, Point& b){
			// 	return a.x < b.x;
			// });
			// std::sort(childVoxels.begin(), childVoxels.end(), [](Voxel& a, Voxel& b){
			// 	return a.x < b.x;
			// });

			Vector3 nodeSize = node->max - node->min;
			for(Point point : childPoints){

				float fvx = fGridSize * (point.x - node->min.x) / nodeSize.x;
				float fvy = fGridSize * (point.y - node->min.y) / nodeSize.y;
				float fvz = fGridSize * (point.z - node->min.z) / nodeSize.z;

				int ivx = int(std::clamp(fvx, 0.0f, fGridSize - 1.0f));
				int ivy = int(std::clamp(fvy, 0.0f, fGridSize - 1.0f));
				int ivz = int(std::clamp(fvz, 0.0f, fGridSize - 1.0f));

				for(int ox = ivx; ox <= ivx + 1; ox++)
				for(int oy = ivy; oy <= ivy + 1; oy++)
				for(int oz = ivz; oz <= ivz + 1; oz++)
				{
					if(ox < 0 || ox >= gridSize) continue;
					if(oy < 0 || oy >= gridSize) continue;
					if(oz < 0 || oz >= gridSize) continue;

					float center_vx = float(ox) + 0.5;
					float center_vy = float(oy) + 0.5;
					float center_vz = float(oz) + 0.5;

					float dx = fvx - center_vx;
					float dy = fvy - center_vy;
					float dz = fvz - center_vz;

					float distance = sqrt(dx * dx + dy * dy + dz *dz);

					float weight = std::clamp(1.0 - distance, 0.0, 1.0);

					int voxelIndex = ox + oy * gridSize + oz * gridSize * gridSize;
					grid[voxelIndex * 4 + 0] += weight * point.r;
					grid[voxelIndex * 4 + 1] += weight * point.g;
					grid[voxelIndex * 4 + 2] += weight * point.b;
					grid[voxelIndex * 4 + 3] += weight;
				}
			}
			for(Voxel voxel : childVoxels){
				int voxelIndex = voxel.x + voxel.y * gridSize + voxel.z * gridSize * gridSize;
				grid[voxelIndex * 4 + 0] += voxel.r;
				grid[voxelIndex * 4 + 1] += voxel.g;
				grid[voxelIndex * 4 + 2] += voxel.b;
				grid[voxelIndex * 4 + 3] += voxel.w;
			}

			vector<Voxel> voxels;
			for(int x = 0; x < gridSize; x++)
			for(int y = 0; y < gridSize; y++)
			for(int z = 0; z < gridSize; z++)
			{
				int voxelIndex = x + y * gridSize + z * gridSize * gridSize;

				double r = grid[voxelIndex * 4 + 0];
				double g = grid[voxelIndex * 4 + 1];
				double b = grid[voxelIndex * 4 + 2];
				double w = grid[voxelIndex * 4 + 3];

				if(w > 0.0){
					Voxel voxel = {r, g, b, w, x, y, z};

					voxels.push_back(voxel);
				}
			}
			node->voxels = voxels;

			// write child nodes to disk
			for (int64_t childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {
					continue;
				}

				onNodeCompleted(child.get());
			}

			// node->points = accepted;
			// node->numPoints = numAccepted;

			return true;
		});
	}

};

