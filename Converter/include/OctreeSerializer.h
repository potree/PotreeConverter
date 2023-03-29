#pragma once

#include <format>

using std::format;

#include "structures.h"
#include "Attributes.h"
#include "unsuck/unsuck.hpp"
#include "Vector3.h"
#include "converter_utils.h"
#include "structures.h"

int gridSize = 256;
int dGridSize = gridSize;

struct OctreeSerializer{

	static shared_ptr<Buffer> toVoxelBuffer(Node* node, Node* parent){

		if(node->name == "r"){
			// encode parent coordinates directly
			auto buffer = make_shared<Buffer>(node->voxels.size() * 3);

			for(int i = 0; i < node->voxels.size(); i++){
				Voxel voxel = node->voxels[i];

				buffer->set<uint8_t>(voxel.x, 3 * i + 0);
				buffer->set<uint8_t>(voxel.y, 3 * i + 1);
				buffer->set<uint8_t>(voxel.z, 3 * i + 2);
			}

			return buffer;
		}else if(parent != nullptr){

			int childIndex = node->nodeIndex();
			int cx = (childIndex >> 2) & 1;
			int cy = (childIndex >> 1) & 1;
			int cz = (childIndex >> 0) & 1;

			// auto buffer = make_shared<Buffer>(voxels->size() * 3);

			// int numParentVoxels = 0; 
			int numChildVoxels = node->voxels.size();

			// compute parent-child voxels
			int parent_i = 0;
			uint8_t childmask = 0;
			vector<Voxel> parentVoxels;
			vector<uint8_t> childmasks;
			for(int child_i = 0; child_i < node->voxels.size(); child_i++ ){
			
				Voxel parentVoxelCandidate = parent->voxels[parent_i];
				Voxel childVoxel = node->voxels[child_i];

				int px = (childVoxel.x + cx * gridSize) / 2;
				int py = (childVoxel.y + cy * gridSize) / 2;
				int pz = (childVoxel.z + cz * gridSize) / 2;

				bool isBiologicalFamily = 
					(px == parentVoxelCandidate.x) && 
					(py == parentVoxelCandidate.y) && 
					(pz == parentVoxelCandidate.z);

				if(isBiologicalFamily){

					// first child voxel in parent
					if(childmask == 0){
						parentVoxels.push_back(parentVoxelCandidate);
						childmasks.push_back(childmask);
					}

					// project parent and child voxels to grid in parent node with 2x resolution,
					// then compute parent-to-child index with [0,1) coordinates
					int vcx = (childVoxel.x + cx * gridSize) - parentVoxelCandidate.x * 2;
					int vcy = (childVoxel.y + cy * gridSize) - parentVoxelCandidate.y * 2;
					int vcz = (childVoxel.z + cz * gridSize) - parentVoxelCandidate.z * 2;

					assert(vcx == 0 || vcx == 1);
					assert(vcy == 0 || vcy == 1);
					assert(vcz == 0 || vcz == 1);

					int childVoxelIndex = (vcx << 2) | (vcy << 1) | vcz;

					childmask = childmask | (1 << childVoxelIndex);
					childmasks[childmasks.size() - 1] = childmask;

				}else{
					// wasn't the right parent, try next one
					parent_i++;
					// try next parent without advancing to next child
					child_i--;
					childmask = 0;
				}
			}

			auto buffer = make_shared<Buffer>(childmasks.size());
			memcpy(buffer->data, childmasks.data(), childmasks.size());

			return buffer;
		}else{
			// parent should only be null if <node> is the root node
			printfmt("error {}:{} \n", __FILE__, __LINE__);
			exit(123);
		}

	}

	// Serializes <node->children> relative to voxel coordinates in <node>
	// <node> is serialized only if it is the root node.
	static void serialize(Node* node, Attributes* attributes){

		printfmt("serialize {} \n", node->name);

		auto tStart = now();

		auto process = [](Node* child, Node* parent){
			if(child->numPoints > 0){
				// serialize points
				uint64_t bufferSize = child->numPoints * 16;

				child->serializedBuffer = child->points;
				// node->byteSize = bufferSize;
				// node->byteOffset = byteOffset;
				// byteOffset += node->byteSize;

			}else if(child->numVoxels > 0){
				// serialize voxels
				auto voxelBuffer = toVoxelBuffer(child, parent);

				auto fullBuffer = make_shared<Buffer>(voxelBuffer->size + 3 * child->voxels.size());
				memset(fullBuffer->data, 0, fullBuffer->size);
				memcpy(fullBuffer->data, voxelBuffer->data, voxelBuffer->size);

				// string path = format("G:/temp/proto/{}.voxels", node->name);
				// writeBinaryFile(path, fullBuffer);
				// writeBinaryFile(path, *voxelBuffer);

				child->serializedBuffer = fullBuffer;
				// node->byteSize = fullBuffer->size;
				// node->byteOffset = byteOffset;
				// byteOffset += node->byteSize;

				printfmt("[{:6}] voxels: {:8L}, bytes: {:4} kb \n", child->name, child->voxels.size(), fullBuffer->size / 1000);
			}else{
				// wat
				printfmt("error: no points or voxels. {}:{}\n", __FILE__, __LINE__);
				exit(123);
			}
		};

		for(auto child : node->children){
			if(child == nullptr) continue;

			process(child.get(), node);
		}

		if(node->name == "r"){
			process(node, nullptr);
		}

		 if(node->numPoints > 0){
		 	process(node, nullptr);
		 }


		// node->traverse([&](Node* node){

		// 	if(node->voxels.size() > 0){
		// 		// WRITE VOXEL NODE
		// 		string parentName = node->name.substr(0, node->name.size() - 1);

		// 		vector<Voxel>* voxels = voxelsMap[node->name];
		// 		vector<Voxel>* parentVoxels = nullptr;

		// 		if(voxelsMap.find(parentName) != voxelsMap.end()){
		// 			parentVoxels = voxelsMap[parentName];
		// 		}

		// 		{ // WRITE VOXEL BIN
		// 			auto voxelBuffer = toVoxelBuffer(node, voxels, parentVoxels);

		// 			if(voxelBuffer != nullptr){

		// 				Buffer fullBuffer(voxelBuffer->size + 3 * voxels->size());
		// 				memset(fullBuffer.data, 0, fullBuffer.size);
		// 				memcpy(fullBuffer.data, voxelBuffer->data, voxelBuffer->size);

		// 				string path = format("G:/temp/proto/{}.voxels", node->name);
		// 				writeBinaryFile(path, fullBuffer);
		// 				// writeBinaryFile(path, *voxelBuffer);

		// 				node->byteSize = fullBuffer.size;
		// 				node->byteOffset = byteOffset;
		// 				byteOffset += node->byteSize;

		// 				printfmt("[{:6}] voxels: {:8L}, bytes: {:4} kb \n", node->name, voxels->size(), fullBuffer.size / 1000);
		// 			}
					
		// 		}

		// 		if(false)
		// 		{ // WRITE CSV
		// 			stringstream ss;

		// 			Vector3 min = node->min;
		// 			Vector3 size = node->max - node->min;
		// 			auto scale = attributes->posScale;
		// 			auto offset = attributes->posOffset;
		// 			int offset_rgb = attributes->getOffset("rgb");

		// 			for(auto voxel : *voxels){

		// 				float x = size.x * double(voxel.x) / dGridSize + min.x;
		// 				float y = size.y * double(voxel.y) / dGridSize + min.y;
		// 				float z = size.z * double(voxel.z) / dGridSize + min.z;

		// 				uint16_t R = voxel.r;
		// 				uint16_t G = voxel.g;
		// 				uint16_t B = voxel.b;

		// 				int r = R < 256 ? R : R / 256;
		// 				int g = G < 256 ? G : G / 256;
		// 				int b = B < 256 ? B : B / 256;

		// 				ss << format("{}, {}, {}, {}, {}, {} \n", x, y, z, r, g, b);
		// 			}

		// 			string path = format("G:/temp/proto/{}.csv", node->name);
		// 			writeFile(path, ss.str());
		// 		}

		// 	}else if(node->numPoints > 0){
		// 		// WRITE LEAF NODE

		// 		uint64_t bufferSize = node->numPoints * 16;

		// 		node->byteSize = bufferSize;
		// 		node->byteOffset = byteOffset;
		// 		byteOffset += node->byteSize;

		// 	}

			
		// });

		// int offset_rgb = attributes->getOffset("rgb");

		// for(auto [nodeName, voxels] : *voxelsMap){

		// 	stringstream ss;

		// 	Vector3 min = node->min;
		// 	Vector3 size = node->max - node->min;
		// 	auto scale = attributes->posScale;
		// 	auto offset = attributes->posOffset;

		// 	for(auto voxel : voxels){

		// 		float x = size.x * double(voxel.x) / dGridSize + min.x;
		// 		float y = size.y * double(voxel.y) / dGridSize + min.y;
		// 		float z = size.z * double(voxel.z) / dGridSize + min.z;

		// 		uint16_t R = node->points->get<uint16_t>(attributes->bytes * voxel.pointIndex + offset_rgb + 0);
		// 		uint16_t G = node->points->get<uint16_t>(attributes->bytes * voxel.pointIndex + offset_rgb + 2);
		// 		uint16_t B = node->points->get<uint16_t>(attributes->bytes * voxel.pointIndex + offset_rgb + 4);

		// 		int r = R < 256 ? R : R / 256;
		// 		int g = G < 256 ? G : G / 256;
		// 		int b = B < 256 ? B : B / 256;

		// 		ss << format("{}, {}, {}, {}, {}, {} \n", x, y, z, r, g, b);
		// 	}

		// 	string path = format("G:/temp/proto/{}.csv", node->name);
		// 	writeFile(path, ss.str());
		// }
		


		printElapsedTime(format("duration [{}]: ", node->name), tStart);

	}

};
