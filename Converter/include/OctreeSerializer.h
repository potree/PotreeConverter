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

		Vector3 startP = point - start;
		Vector3 startEnd = end - start;

		double startEnd2 = startEnd.dot(startEnd);
		double startEnd_startP = startEnd.dot(startP);

		double t = startEnd_startP / startEnd2;

		t = t < 0.0 ? 0.0 : t;
		t = t > 1.0 ? 1.0 : t;

		return t;
	}

	static shared_ptr<Buffer> compress(shared_ptr<Buffer> buffer){

		shared_ptr<Buffer> out;

		int quality = 3;
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

				printfmt("WARNING: reserved encoded_buffer size was too small. Trying again with size {}. \n", formatNumber(encoded_size));
			}
		}

		if (success == BROTLI_FALSE) {
			printfmt("ERROR: failed to compress node. aborting conversion \n");

			exit(123);
		}

		out = make_shared<Buffer>(encoded_size);
		memcpy(out->data, encoded_buffer, encoded_size);
	
		return out;
	}

	static shared_ptr<Buffer> toVoxelCoordBuffer(Node* node, Node* parent, Attributes* attributes){

		int numVoxels = node->numVoxels;

		if(node->name == "r"){
			// encode parent coordinates directly
			int bytesPerVoxel = 3;
			auto buffer = make_shared<Buffer>(numVoxels * bytesPerVoxel);

			for(int i = 0; i < numVoxels; i++){
				Voxel voxel = node->voxels[i];

				buffer->set<uint8_t>(voxel.x, 3 * i + 0);
				buffer->set<uint8_t>(voxel.y, 3 * i + 1);
				buffer->set<uint8_t>(voxel.z, 3 * i + 2);
			}

			 return buffer;
		}else if(parent != nullptr){
			// encode inner node voxel coordinates relative to parent

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

			auto buffer = make_shared<Buffer>(childmasks.size());

			// XYZ
			memcpy(buffer->data, childmasks.data(), childmasks.size());

			return buffer;
		}else{
			// parent should only be null if <node> is the root node
			printfmt("error {}:{} \n", __FILE__, __LINE__);
			exit(123);
		}

	}

	static shared_ptr<Buffer> compress_bc(vector<Voxel>& voxels){

		int numVoxels = voxels.size();

		//  BC-ish encoded voxels
		int blocksize = 8;
		int numSamples = 4;
		int bitsPerSample = 2;
		int numBlocks = (numVoxels + blocksize - 1) / blocksize;
		int bitsPerBlock = (48 + blocksize * bitsPerSample);
		int bytesPerBlock = 8; // with this specific settings...
		int numBits = bitsPerBlock * numBlocks;
		int numBytes = numBits / 8;
		// memset(target.data, 0, target.size);
		auto buffer = make_shared<Buffer>(numBytes);
		memset(buffer->data, 0, buffer->size);

		vector<Vector3> starts;
		vector<Vector3> ends;
		
		// compute line endpoints
		for(int i = 0; i < numVoxels; i++){

			if((i % blocksize) == 0){
				// new block
				Vector3 min = {Infinity, Infinity, Infinity};
				Vector3 max = {-Infinity, -Infinity, -Infinity};

				starts.push_back(min);
				ends.push_back(max);
			}

			int blockIndex = i / blocksize;
			Voxel voxel = voxels[i];
			
			starts[blockIndex].x = std::min(starts[blockIndex].x, double(voxel.r));
			starts[blockIndex].y = std::min(starts[blockIndex].y, double(voxel.g));
			starts[blockIndex].z = std::min(starts[blockIndex].z, double(voxel.b));
			ends[blockIndex].x = std::max(ends[blockIndex].x, double(voxel.r));
			ends[blockIndex].y = std::max(ends[blockIndex].y, double(voxel.g));
			ends[blockIndex].z = std::max(ends[blockIndex].z, double(voxel.b));
		}

		// store endpoints in buffer
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
			buffer->set<uint8_t>(start_r, blockOffset + 0);
			buffer->set<uint8_t>(start_g, blockOffset + 1);
			buffer->set<uint8_t>(start_b, blockOffset + 2);
			buffer->set<uint8_t>(end_r, blockOffset + 3);
			buffer->set<uint8_t>(end_g, blockOffset + 4);
			buffer->set<uint8_t>(end_b, blockOffset + 5);
		}

		// encode voxel colors along line
		for(int i = 0; i < numVoxels; i++){

			int blockIndex = i / blocksize;
			Vector3 start = starts[blockIndex];
			Vector3 end = ends[blockIndex];

			Voxel voxel = voxels[i];
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
			uint16_t bits = buffer->get<uint16_t>(blockOffset + 6);
			bits = bits | T << (bitsPerSample * blockSampleIndex);
			buffer->set<uint16_t>(bits, blockOffset + 6);
		}

		return buffer;
	}

	static shared_ptr<Buffer> toFilteredBuffer(Node* node, Attributes* attributes){

		// int numVoxels = node->numVoxels;

		// if(node->name == "r"){
		// 	// store root colors as 3 x uint8
		// 	int bytesPerVoxel = 3;

		// 	auto buffer = make_shared<Buffer>(3 * numVoxels);

		// 	for(int i = 0; i < numVoxels; i++){
		// 		Voxel voxel = node->voxels[i];

		// 		buffer->set<uint8_t>(voxel.r, 3 * i + 0);
		// 		buffer->set<uint8_t>(voxel.g, 3 * i + 1);
		// 		buffer->set<uint8_t>(voxel.b, 3 * i + 2);
		// 	}

		// 	 return buffer;
		// }else{

		// 	auto buffer = compress_bc(node->voxels);

		// 	return buffer;
		// }

		auto buffer = compress_bc(node->voxels);

		return buffer;

	}

	static shared_ptr<Buffer> toUnfilteredBuffer(Node* node, Attributes* attributes){

		int unfilteredAttributesSize = 0;
		vector<bool> unfilteredAttributeMask;
		vector<int> unfilteredOffsets;

		int unfilteredOffset = 0;
		for(auto& attribute : attributes->list){
			bool isUnfiltered = false;

			if(attribute.name == "position"){
				isUnfiltered = false;
			}else if(attribute.name == "rgb"){
				isUnfiltered = false;
			}else{
				isUnfiltered = true;
				unfilteredAttributesSize += attribute.size;
			}

			unfilteredAttributeMask.push_back(isUnfiltered);

			unfilteredOffsets.push_back(unfilteredOffset);

			if(isUnfiltered){
				unfilteredOffset += attribute.size;
			}
		}

		int numVoxels = node->numVoxels;

		auto buffer = make_shared<Buffer>(node->unfilteredVoxelData->size);

		for(int voxelIndex = 0; voxelIndex < node->numVoxels; voxelIndex++){

			for(int j = 0; j < attributes->list.size(); j++){
				Attribute& attribute = attributes->list[j];
				bool isUnfiltered = unfilteredAttributeMask[j];

				if(isUnfiltered){

					memcpy(
						buffer->data_u8 + unfilteredOffsets[j] * numVoxels + voxelIndex * attribute.size, 
						node->unfilteredVoxelData->data_u8 + voxelIndex * unfilteredAttributesSize + unfilteredOffsets[j],
						attribute.size
					);
				}
			}

		}

		auto compressedBuffer = OctreeSerializer::compress(buffer);
			
		return compressedBuffer;
	}

	static shared_ptr<Buffer> toPointsBuffer(Node* node, Attributes* inputAttributes, Attributes* outputAttributes){

		struct Mapping{
			int64_t offset_source;
			int64_t offset_target;
			int64_t byteSize;
		};
		vector<Mapping> mappings;

		for(int i_in = 0; i_in < inputAttributes->list.size(); i_in++){
			Attribute attribute_in = inputAttributes->list[i_in];
			Attribute* attribute_out = outputAttributes->get(attribute_in.name);

			if(attribute_out != nullptr){
				Mapping mapping;
				mapping.offset_source = inputAttributes->getOffset(attribute_in.name);
				mapping.offset_target = outputAttributes->getOffset(attribute_in.name);
				mapping.byteSize = attribute_in.size;
				mappings.push_back(mapping);
			}
		}

		int64_t numBytes = outputAttributes->bytes * node->numPoints;
		auto prunedData = make_shared<Buffer>(numBytes);

		auto pruned_soa = make_shared<Buffer>(numBytes);

		int stride_in = inputAttributes->bytes;
		int stride_out = outputAttributes->bytes;

		for(int i = 0; i < node->numPoints; i++){

			for(Mapping& mapping : mappings){
				int offset_source = i * stride_in + mapping.offset_source;
				int offset_target = i * stride_out + mapping.offset_target;

				memcpy(prunedData->data_u8 + offset_target, node->points->data_u8 + offset_source, mapping.byteSize);



				{ // DEBUG
					int offset_attributes = node->numPoints * mapping.offset_target;
					int offset_target = offset_attributes + i * mapping.byteSize;

					memcpy(pruned_soa->data_u8 + offset_target, node->points->data_u8 + offset_source, mapping.byteSize);
				}
			}
		}

		// auto compressed_aos = OctreeSerializer::compress(prunedData);
		auto compressed_soa = OctreeSerializer::compress(pruned_soa);

		// // int compressionRate = (100ull * compressed->size) / node->points->size;

		// static int64_t bytes_uncompressed = 0;
		// static atomic_int64_t bytes_aos = 0;
		// static atomic_int64_t bytes_soa = 0;

		// { // DEBUG
		// 	auto compressed_soa = OctreeSerializer::compress(pruned_soa);

		// 	bytes_uncompressed += node->points->size;
		// 	bytes_aos += compressed->size;
		// 	bytes_soa += compressed_soa->size;

		// 	int compressionRate_aos = (100ull * bytes_aos) / bytes_uncompressed;
		// 	int compressionRate_soa = (100ull * bytes_soa) / bytes_uncompressed;
		// 	// int compressionRate_aos = (100ull * compressed->size) / node->points->size;
		// 	// int compressionRate_soa = (100ull * compressed_soa->size) / node->points->size;

		// 	cout << format("aos: {:4}%, soa: {:4}% \n", compressionRate_aos, compressionRate_soa);
		// 	// cout << format("aos: {:4}%, soa: {:4}% \n", compressionRate_aos, compressionRate_soa);


		// }

		return compressed_soa;
	}

	// Serializes <node->children> relative to voxel coordinates in <node>
	// <node> is serialized only if it is the root node.
	static void serialize(Node* node, Attributes* inputAttributes, Attributes* outputAttributes){

		// printfmt("serialize {} \n", node->name);

		auto tStart = now();

		auto process = [inputAttributes, outputAttributes](Node* child, Node* parent){

			static atomic<uint64_t> numNodesSerialized = atomic<uint64_t>(0);

			if(child->numPoints > 0){
				// serialize points
				auto pointsBuffer = toPointsBuffer(child, inputAttributes, outputAttributes);

				child->serializedPoints = pointsBuffer;
				child->serializationIndex = numNodesSerialized++;
				child->sPointsSize = pointsBuffer->size;

				// printfmt("[{:6}] points: {:8L}, bytes: {:4} kb \n", child->name, child->numPoints, pointsBuffer->size / 1000);
			}else if(child->numVoxels > 0){
				// serialize voxels

				child->serializedPosition   = toVoxelCoordBuffer(child, parent, inputAttributes);
				child->serializedFiltered   = toFilteredBuffer(child, inputAttributes);
				child->serializedUnfiltered = toUnfilteredBuffer(child, inputAttributes);

				// { // DEBUG

				// 	static mutex mtx;

				// 	auto compressed_position = OctreeSerializer::compress(child->serializedPosition);
				// 	auto compressed_filtered = OctreeSerializer::compress(child->serializedFiltered);
				// 	auto compressed_unfiltered = OctreeSerializer::compress(child->serializedUnfiltered);

				// 	// float rate_position   = 100.0f * float(compressed_position->size) / float(child->serializedPosition->size);
				// 	// float rate_filtered   = 100.0f * float(compressed_filtered->size) / float(child->serializedFiltered->size);
				// 	// float rate_unfiltered = 100.0f * float(compressed_unfiltered->size) / float(child->serializedUnfiltered->size);

				// 	//printfmt("compression: {:4.0f} - {:4.0f} - {:4.0f} \n", rate_position, rate_filtered, rate_unfiltered);

				// 	static int64_t sum_uncomp_position = 0;
				// 	static int64_t sum_uncomp_filtered = 0;
				// 	static int64_t sum_uncomp_unfiltered = 0;
				// 	static int64_t sum_comp_position = 0;
				// 	static int64_t sum_comp_filtered = 0;
				// 	static int64_t sum_comp_unfiltered = 0;

				// 	mtx.lock();

				// 	sum_uncomp_position   += child->serializedPosition->size;
				// 	sum_uncomp_filtered   += child->serializedFiltered->size;
				// 	sum_uncomp_unfiltered += child->serializedUnfiltered->size;
				// 	sum_comp_position     += compressed_position->size;
				// 	sum_comp_filtered     += compressed_filtered->size;
				// 	sum_comp_unfiltered   += compressed_unfiltered->size;

				// 	double rate_position   = 100.0 * double(sum_comp_position)   / double(sum_uncomp_position);
				// 	double rate_filtered   = 100.0 * double(sum_comp_filtered)   / double(sum_uncomp_filtered);
				// 	double rate_unfiltered = 100.0 * double(sum_comp_unfiltered) / double(sum_uncomp_unfiltered);

				// 	printfmt("=========================\n");
				// 	printfmt("sizes:             {:12L} - {:12L} - {:12L} \n", sum_uncomp_position, sum_uncomp_filtered, sum_uncomp_unfiltered);
				// 	printfmt("compressed sizes:  {:12L} - {:12L} - {:12L} \n", sum_comp_position, sum_comp_filtered, sum_comp_unfiltered);
				// 	printfmt("compression:       {:4.0f} - {:4.0f} - {:4.0f} \n", rate_position, rate_filtered, rate_unfiltered);

				// 	mtx.unlock();
				// }

				child->sPositionSize   = child->serializedPosition->size;
				child->sFilteredSize   = child->serializedFiltered->size;
				child->sUnfilteredSize = child->serializedUnfiltered->size;

				child->serializationIndex = numNodesSerialized++;

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
