#pragma once

#include <format>

using std::format;

#include "structures.h"
#include "Attributes.h"
#include "unsuck/unsuck.hpp"
#include "Vector3.h"
#include "converter_utils.h"


struct OctreeSerializer{

	static void serialize(Node* node, Attributes* attributes){

		cout << format("serialize {} \n", node->name);

		int gridSize = 256;
		int fGridSize = gridSize;

		// - traverse top-bottom
		// - morton-order points/voxels
		// - if inner node, create ordered list of voxels
		// - if leaf node, just write points without ordering

		auto tStart = now();

		struct Voxel{
			uint8_t x;
			uint8_t y;
			uint8_t z;
			uint8_t padding;
			uint32_t mortonCode;
			uint32_t pointIndex;
		};

		unordered_map<Node*, vector<Voxel>> voxelsMap;

		node->traverse([&](Node* node){
			
			if(node->isLeaf()){

			}else{

				int stride = attributes->bytes;
				Vector3 scale = attributes->posScale;
				Vector3 offset = attributes->posOffset;
				Vector3 nodesize = node->max - node->min;

				vector<Voxel> voxels;
				voxels.reserve(node->numPoints);

				for(int i = 0; i < node->numPoints; i++){

					int X = node->points->get<int32_t>(i * stride + 0);
					int Y = node->points->get<int32_t>(i * stride + 4);
					int Z = node->points->get<int32_t>(i * stride + 8);

					double x = (double(X) * scale.x) + offset.x;
					double y = (double(Y) * scale.y) + offset.y;
					double z = (double(Z) * scale.z) + offset.z;

					uint32_t ix = fGridSize * ((x - node->min.x) / nodesize.x);
					uint32_t iy = fGridSize * ((y - node->min.y) / nodesize.y);
					uint32_t iz = fGridSize * ((z - node->min.z) / nodesize.z);

					// PROTO
					if(ix > 255) exit(534);
					if(iy > 255) exit(534);
					if(iz > 255) exit(534);

					Voxel voxel;
					voxel.x = ix;
					voxel.y = iy;
					voxel.z = iz;
					voxel.pointIndex = i;
					voxel.mortonCode = mortonEncode_magicbits(ix, iy, iz);

					voxels.push_back(voxel);
				}

				sort(voxels.begin(), voxels.end(), [](Voxel& a, Voxel& b){
					return a.mortonCode < b.mortonCode;
				});

				voxelsMap[node] = voxels;
			}

		});


		printElapsedTime(format("duration [{}]: ", node->name), tStart);

	}

};
