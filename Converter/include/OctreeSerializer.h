#pragma once

#include <format>

using std::format;

#include "structures.h"
#include "Attributes.h"
#include "unsuck/unsuck.hpp"
#include "Vector3.h"
#include "converter_utils.h"


struct OctreeSerializer{

	struct Voxel{
		uint8_t x;
		uint8_t y;
		uint8_t z;
		uint8_t padding;
		uint32_t mortonCode;
		uint32_t pointIndex;
	};

	static shared_ptr<unordered_map<Node*, vector<Voxel>>> create_morton_ordered_voxel_map(
		Node* node, Attributes* attributes, int gridSize
	){

		double dGridSize = gridSize;
		auto voxelsMap = make_shared<unordered_map<Node*, vector<Voxel>>>();

		node->traverse([&](Node* node){
			
			if(node->isLeaf()){
				// no voxels in leaves
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

					uint32_t ix = dGridSize * ((x - node->min.x) / nodesize.x);
					uint32_t iy = dGridSize * ((y - node->min.y) / nodesize.y);
					uint32_t iz = dGridSize * ((z - node->min.z) / nodesize.z);

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

				voxelsMap->operator[](node) = voxels;
			}

		});

		return voxelsMap;
	}

	static void serialize(Node* node, Attributes* attributes){

		cout << format("serialize {} \n", node->name);

		int gridSize = 256;
		int dGridSize = gridSize;

		// - traverse top-bottom
		// - morton-order points/voxels
		// - if inner node, create ordered list of voxels
		// - if leaf node, just write points without ordering

		auto tStart = now();

		auto voxelsMap = create_morton_ordered_voxel_map(node, attributes, gridSize);

		int offset_rgb = attributes->getOffset("rgb");

		for(auto [node, voxels] : *voxelsMap){

			stringstream ss;

			Vector3 min = node->min;
			Vector3 size = node->max - node->min;
			auto scale = attributes->posScale;
			auto offset = attributes->posOffset;

			for(auto voxel : voxels){

				float x = size.x * double(voxel.x) / dGridSize + min.x;
				float y = size.y * double(voxel.y) / dGridSize + min.y;
				float z = size.z * double(voxel.z) / dGridSize + min.z;

				uint16_t R = node->points->get<uint16_t>(attributes->bytes * voxel.pointIndex + offset_rgb + 0);
				uint16_t G = node->points->get<uint16_t>(attributes->bytes * voxel.pointIndex + offset_rgb + 2);
				uint16_t B = node->points->get<uint16_t>(attributes->bytes * voxel.pointIndex + offset_rgb + 4);

				int r = R < 256 ? R : R / 256;
				int g = G < 256 ? G : G / 256;
				int b = B < 256 ? B : B / 256;

				ss << format("{}, {}, {}, {}, {}, {} \n", x, y, z, r, g, b);
			}

			string path = format("G:/temp/proto/{}.csv", node->name);
			writeFile(path, ss.str());
		}
		


		printElapsedTime(format("duration [{}]: ", node->name), tStart);

	}

};
