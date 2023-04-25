#pragma once

#include <format>

using std::format;

#include "structures.h"
#include "Attributes.h"
#include "unsuck/unsuck.hpp"
#include "Vector3.h"
#include "converter_utils.h"
#include "structures.h"

int gridSize = 128;
int dGridSize = gridSize;

struct OctreeSerializer{

	// Adapted from three.js
	// license: MIT (https://github.com/mrdoob/three.js/blob/dev/LICENSE)
	// url: https://github.com/mrdoob/three.js/blob/dev/src/math/Line3.js
	static float closestPointToPointParameter(Vector3 start, Vector3 end, Vector3 point){

		// _startP.subVectors( point, this.start );
		// _startEnd.subVectors( this.end, this.start );

		Vector3 startP = point - start;
		Vector3 startEnd = end - start;

		double startEnd2 = startEnd.dot(startEnd);
		double startEnd_startP = startEnd.dot(startP);

		// const startEnd2 = _startEnd.dot( _startEnd );
		// const startEnd_startP = _startEnd.dot( _startP );

		// let t = startEnd_startP / startEnd2;

		double t = startEnd_startP / startEnd2;

		t = t < 0.0 ? 0.0 : t;
		t = t > 1.0 ? 1.0 : t;

		// if ( clampToLine ) {

		// 	t = MathUtils.clamp( t, 0, 1 );

		// }

		return t;

	}

	static shared_ptr<Buffer> compress(shared_ptr<Buffer> buffer){

		shared_ptr<Buffer> out;

		int quality = 6;
		int lgwin = BROTLI_DEFAULT_WINDOW;
		auto mode = BROTLI_DEFAULT_MODE;
		uint8_t* input_buffer = buffer->data_u8;
		size_t input_size = buffer->size;

		size_t encoded_size = input_size * 1.5 + 1'000;
		shared_ptr<Buffer> outputBuffer = make_shared<Buffer>(encoded_size);
		uint8_t* encoded_buffer = outputBuffer->data_u8;

		BROTLI_BOOL success = BROTLI_FALSE;

		for (int i = 0; i < 5; i++) {
			success = BrotliEncoderCompress(quality, lgwin, mode, input_size, input_buffer, &encoded_size, encoded_buffer);

			if (success == BROTLI_TRUE) {
				break;
			} else {
				encoded_size = (encoded_size + 1024) * 1.5;
				outputBuffer = make_shared<Buffer>(encoded_size);
				encoded_buffer = outputBuffer->data_u8;

				logger::WARN("reserved encoded_buffer size was too small. Trying again with size " + formatNumber(encoded_size) + ".");
			}
		}

		if (success == BROTLI_FALSE) {
			stringstream ss;
			ss << "failed to compress node. aborting conversion." ;
			logger::ERROR(ss.str());

			exit(123);
		}

		out = make_shared<Buffer>(encoded_size);
		memcpy(out->data, encoded_buffer, encoded_size);
	
		return out;
	}

	static shared_ptr<Buffer> toVoxelBuffer(Node* node, Node* parent, Attributes* attributes){

		int numVoxels = node->numVoxels;

		auto aRGB = attributes->get("rgb");
		int aRGB_bytesize = (aRGB == nullptr) ? 0 : 6;
		int aXYZ_bytesize = 12;

		shared_ptr<Buffer> sourcedata;
		if(node->points){
			sourcedata = node->points;
		}else if(node->voxeldata){
			sourcedata = node->voxeldata;
		}

		if(node->name == "r"){
			// encode parent coordinates directly
			int stride_rest = attributes->bytes - aXYZ_bytesize;

			int bytesPerVoxel = 3 + 3; // xyz + rgb
			int bytesize_xyzrgb = numVoxels * bytesPerVoxel;
			int bytesize_rest = numVoxels * stride_rest;

			auto buffer = make_shared<Buffer>(bytesize_xyzrgb + bytesize_rest);
			int t_bytesize_xyz = numVoxels * 3;
			int t_bytesize_rgb = numVoxels * 3;
			int t_offset_rgb = t_bytesize_xyz;
			int t_offset_rest = t_offset_rgb + t_bytesize_rgb;

			for(int i = 0; i < numVoxels; i++){
				Voxel voxel = node->voxels[i];

				buffer->set<uint8_t>(voxel.x, 3 * i + 0);
				buffer->set<uint8_t>(voxel.y, 3 * i + 1);
				buffer->set<uint8_t>(voxel.z, 3 * i + 2);

				buffer->set<uint8_t>(voxel.r, t_offset_rgb + 3 * i + 0);
				buffer->set<uint8_t>(voxel.g, t_offset_rgb + 3 * i + 1);
				buffer->set<uint8_t>(voxel.b, t_offset_rgb + 3 * i + 2);

				//memcpy(
				//	buffer->data_u8 + t_offset_rest + i * stride_rest, 
				//	sourcedata->data_u8 + i * attributes->bytes + aXYZ_bytesize,
				//	stride_rest
				//);
			}

			 return buffer;
			//return OctreeSerializer::compress(buffer);
		}else if(parent != nullptr){

			int childIndex = node->nodeIndex();
			int cx = (childIndex >> 2) & 1;
			int cy = (childIndex >> 1) & 1;
			int cz = (childIndex >> 0) & 1;

			// compute parent-child voxels
			int parent_i = 0;
			uint8_t childmask = 0;
			vector<Voxel> parentVoxels;
			vector<uint8_t> childmasks;
			for(int child_i = 0; child_i < numVoxels; child_i++ ){
			
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

			// DEBUG: redundant sanity check
			// int sumbits = 0;
			// for(uint8_t childmask : childmasks){
			// 	int numbits = popcount(childmask);
			// 	sumbits += numbits;
			// }
			// if(node->name == "r240045"){
			// 	int a = 10;
			// }
			// assert(sumbits == numVoxels);

			int t_stride_rest = (attributes->bytes - 12);

			int t_bytesize_xyz = childmasks.size();
			int t_bytesize_rgb = 3 * numVoxels;
			int t_bytesize = t_bytesize_xyz + t_bytesize_rgb;

			int t_offset_rgb = t_bytesize_xyz;
			int t_offset_rest = t_offset_rgb + t_bytesize_rgb;

			auto buffer_xyz = make_shared<Buffer>(t_bytesize_xyz);

			// XYZ
			memcpy(buffer_xyz->data, childmasks.data(), childmasks.size());

			// RGB
			// for(int i = 0; i < numVoxels; i++){
			// 	Voxel voxel = node->voxels[i];
			// 	buffer->set<uint8_t>(voxel.r, t_offset_rgb + 3 * i + 0);
			// 	buffer->set<uint8_t>(voxel.g, t_offset_rgb + 3 * i + 1);
			// 	buffer->set<uint8_t>(voxel.b, t_offset_rgb + 3 * i + 2);
			// }

			// PROTO: BC-ish encoded voxels
			int blocksize = 8;
			int numSamples = 4;
			int bitsPerSample = 2;
			int numBlocks = (numVoxels + blocksize - 1) / blocksize;
			int bitsPerBlock = (48 + blocksize * bitsPerSample);
			int bytesPerBlock = 8; // with this specific settings...
			int numBits = bitsPerBlock * numBlocks;
			int numBytes = numBits / 8;
			Buffer encoded_rgb(numBytes);
			memset(encoded_rgb.data, 0, encoded_rgb.size);

			{ 
				vector<Vector3> starts;
				vector<Vector3> ends;
				

				for(int i = 0; i < numVoxels; i++){

					if((i % blocksize) == 0){
						// new block
						Vector3 min = {Infinity, Infinity, Infinity};
						Vector3 max = {-Infinity, -Infinity, -Infinity};

						starts.push_back(min);
						ends.push_back(max);
					}

					int blockIndex = i / blocksize;
					Voxel voxel = node->voxels[i];
					
					starts[blockIndex].x = std::min(starts[blockIndex].x, double(voxel.r));
					starts[blockIndex].y = std::min(starts[blockIndex].y, double(voxel.g));
					starts[blockIndex].z = std::min(starts[blockIndex].z, double(voxel.b));
					ends[blockIndex].x = std::max(ends[blockIndex].x, double(voxel.r));
					ends[blockIndex].y = std::max(ends[blockIndex].y, double(voxel.g));
					ends[blockIndex].z = std::max(ends[blockIndex].z, double(voxel.b));
				}

				// store endpoints
				for(int blockIndex = 0; blockIndex < numBlocks; blockIndex++){
					Vector3 start = starts[blockIndex];
					Vector3 end = ends[blockIndex];

					uint8_t start_r = uint8_t(start.x > 255.0 ? start.x / 256.0 : start.x);
					uint8_t start_g = uint8_t(start.y > 255.0 ? start.y / 256.0 : start.y);
					uint8_t start_b = uint8_t(start.z > 255.0 ? start.z / 256.0 : start.z);

					uint8_t end_r = uint8_t(end.x > 255.0 ? end.x / 256.0 : end.x);
					uint8_t end_g = uint8_t(end.y > 255.0 ? end.y / 256.0 : end.y);
					uint8_t end_b = uint8_t(end.z > 255.0 ? end.z / 256.0 : end.z);

					int blockOffset = bytesPerBlock * blockIndex;
					encoded_rgb.set<uint8_t>(start_r, blockOffset + 0);
					encoded_rgb.set<uint8_t>(start_g, blockOffset + 1);
					encoded_rgb.set<uint8_t>(start_b, blockOffset + 2);
					encoded_rgb.set<uint8_t>(end_r, blockOffset + 3);
					encoded_rgb.set<uint8_t>(end_g, blockOffset + 4);
					encoded_rgb.set<uint8_t>(end_b, blockOffset + 5);
				}

				for(int i = 0; i < numVoxels; i++){

					int blockIndex = i / blocksize;
					Vector3 start = starts[blockIndex];
					Vector3 end = ends[blockIndex];

					Voxel voxel = node->voxels[i];
					Vector3 point;
					point.x = voxel.r;
					point.y = voxel.g;
					point.z = voxel.b;
					
					float t = OctreeSerializer::closestPointToPointParameter(start, end, point);

					int T = std::round(t * float(numSamples));
					T = std::min(T, numSamples - 1);
					T = std::max(T, 0);


					int blockOffset = bytesPerBlock * blockIndex;

					int blockSampleIndex = i % blocksize;
					uint16_t bits = encoded_rgb.get<uint16_t>(blockOffset + 6);
					bits = bits | T << (bitsPerSample * blockSampleIndex);
					encoded_rgb.set<uint16_t>(bits, blockOffset + 6);
				}

				// memcpy(buffer->data_u8 + t_offset_rgb, encoded.data_u8, encoded.size);
			}

			auto buffer = make_shared<Buffer>(buffer_xyz->size + encoded_rgb.size);
			memcpy(buffer->data_u8, buffer_xyz->data_u8, buffer_xyz->size);
			memcpy(buffer->data_u8 + buffer_xyz->size, encoded_rgb.data_u8, encoded_rgb.size);

			//return OctreeSerializer::compress(buffer);
			return buffer;
		}else{
			// parent should only be null if <node> is the root node
			printfmt("error {}:{} \n", __FILE__, __LINE__);
			exit(123);
		}

	}

	static shared_ptr<Buffer> toRestBuffer(Node* node, Attributes* attributes){

		int numVoxels = node->numVoxels;
		int t_stride_rest = (attributes->bytes - 12);
		int t_bytesize_rest = t_stride_rest * numVoxels;

		auto buffer = make_shared<Buffer>(t_bytesize_rest);

		// TODO

		return buffer;
	}

	static shared_ptr<Buffer> toPointsBuffer(Node* node, Attributes* attributes){

		
		 return node->points;

		//auto buffer = make_shared<Buffer>(node->numPoints * 16);
		//int offsetRGB = attributes->getOffset("rgb");
		//int stride = attributes->bytes;
		//auto scale = attributes->posScale;
		//auto offset = attributes->posOffset;

		//for(int i = 0; i < node->numPoints; i++){
		//	
		//	int32_t X = node->points->get<int32_t>(stride * i + 0);
		//	int32_t Y = node->points->get<int32_t>(stride * i + 4);
		//	int32_t Z = node->points->get<int32_t>(stride * i + 8);

		//	float x = double(X) * scale.x; //+ offset.x;
		//	float y = double(Y) * scale.y; //+ offset.y;
		//	float z = double(Z) * scale.z; //+ offset.z;

		//	buffer->set<float>(x, 16 * i + 0);
		//	buffer->set<float>(y, 16 * i + 4);
		//	buffer->set<float>(z, 16 * i + 8);

		//	int R = node->points->get<uint16_t>(stride * i + offsetRGB + 0);
		//	int G = node->points->get<uint16_t>(stride * i + offsetRGB + 2);
		//	int B = node->points->get<uint16_t>(stride * i + offsetRGB + 4);

		//	uint8_t r = R < 256 ? R : R / 256;
		//	uint8_t g = G < 256 ? G : G / 256;
		//	uint8_t b = B < 256 ? B : B / 256;

		//	buffer->set<uint8_t>(r, 16 * i + 12);
		//	buffer->set<uint8_t>(g, 16 * i + 13);
		//	buffer->set<uint8_t>(b, 16 * i + 14);
		//}

		//return buffer;
	}

	// Serializes <node->children> relative to voxel coordinates in <node>
	// <node> is serialized only if it is the root node.
	static void serialize(Node* node, Attributes* attributes){

		// printfmt("serialize {} \n", node->name);

		auto tStart = now();

		auto process = [attributes](Node* child, Node* parent){

			if(child->numPoints > 0){
				// serialize points
				auto pointsBuffer = toPointsBuffer(child, attributes);

				child->serializedBuffer = pointsBuffer;

				// printfmt("[{:6}] points: {:8L}, bytes: {:4} kb \n", child->name, child->numPoints, pointsBuffer->size / 1000);
			}else if(child->numVoxels > 0){
				// serialize voxels
				auto voxelBuffer = toVoxelBuffer(child, parent, attributes);

				child->serializedBuffer = voxelBuffer;

				// printfmt("[{:6}] voxels: {:8L}, bytes: {:4} kb \n", child->name, child->numVoxels, voxelBuffer->size / 1000);
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
	}

};
