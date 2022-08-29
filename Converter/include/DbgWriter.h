
#pragma once

#include <filesystem>
#include <limits>

#include "structures.h"

namespace fs = std::filesystem;

namespace dbgwriter{

	struct Point {
		int64_t x;
		int64_t y;
		int64_t z;
	};


	vector<Point> getPoints(Node* node, Attributes attributes) {

		vector<Point> points;

		for (int64_t i = 0; i < node->numPoints; i++) {
			int64_t offset = i * attributes.bytes;

			int32_t sX, sY, sZ;
			memcpy(&sX, node->points->data_u8 + offset + 0, 4);
			memcpy(&sY, node->points->data_u8 + offset + 4, 4);
			memcpy(&sZ, node->points->data_u8 + offset + 8, 4);

			Point p;
			p.x = sX;
			p.y = sY;
			p.z = sZ;

			points.push_back(p);

		}

		return points;
	}

	std::pair<Point, Point> computeBox(vector<Point> points) {

		Point min, max;

		min.x = std::numeric_limits<int64_t>::max();
		min.y = std::numeric_limits<int64_t>::max();
		min.z = std::numeric_limits<int64_t>::max();
		max.x = std::numeric_limits<int64_t>::min();
		max.y = std::numeric_limits<int64_t>::min();
		max.z = std::numeric_limits<int64_t>::min();

		for (Point& point : points) {
			min.x = std::min(min.x, point.x);
			min.y = std::min(min.y, point.y);
			min.z = std::min(min.z, point.z);

			max.x = std::max(max.x, point.x);
			max.y = std::max(max.y, point.y);
			max.z = std::max(max.z, point.z);
		}

		return { min, max };
	}

	//struct PNode {

	//	vector<PNode*> children;
	//	Point min;
	//	Point max;

	//	bool isLeaf = true;
	//	vector<Point> points;
	//	int maxPoints = 20;

	//	PNode() {
	//		children.resize(8, nullptr);
	//	}

	//	void add(Point point) {

	//		if (isLeaf) {
	//			points.push_back(point);

	//			if (points.size() > maxPoints) {
	//				split();
	//			}
	//		} else {

	//		}

	//	}

	//	void split() {

	//	}

	//};

	//void writePositionTree(string dir, Node* node, Attributes attributes) {

	//	auto points = getPoints(node, attributes);
	//	auto [min, max] = computeBox(points);

	//	Point range;
	//	range.x = max.x - min.x;
	//	range.y = max.y - min.y;
	//	range.z = max.z - min.z;

	//	for (Point& point : points) {
	//		point.x = point.x - min.x;
	//		point.y = point.y - min.y;
	//		point.z = point.z - min.z;
	//	}

	//	// round to nearest power of two
	//	range.x = std::pow(2, std::ceil(std::log2(range.x)));
	//	range.y = std::pow(2, std::ceil(std::log2(range.y)));
	//	range.y = std::pow(2, std::ceil(std::log2(range.z)));

	//	int64_t cubeSize = std::max(range.x, std::max(range.y, range.z));

	//	PNode* root = new PNode();
	//	root->min = min;
	//	root->max = min;
	//	root->max.x += cubeSize - 1;
	//	root->max.y += cubeSize - 1;
	//	root->max.z += cubeSize - 1;

	//	for (Point& point : points) {
	//		root->add(point);
	//	}


	//}

	void writePosition2(string dir, Node* node, Attributes attributes) {
		
		int64_t numPoints = node->numPoints;

		int64_t minX = std::numeric_limits<int64_t>::max();
		int64_t minY = std::numeric_limits<int64_t>::max();
		int64_t minZ = std::numeric_limits<int64_t>::max();
		int64_t maxX = std::numeric_limits<int64_t>::min();
		int64_t maxY = std::numeric_limits<int64_t>::min();
		int64_t maxZ = std::numeric_limits<int64_t>::min();

		for (int64_t i = 1; i < numPoints; i++) {
			int64_t offset = i * attributes.bytes;

			int32_t sX, sY, sZ;
			memcpy(&sX, node->points->data_u8 + offset + 0, 4);
			memcpy(&sY, node->points->data_u8 + offset + 4, 4);
			memcpy(&sZ, node->points->data_u8 + offset + 8, 4);

			minX = std::min(minX, int64_t(sX));
			minY = std::min(minY, int64_t(sY));
			minZ = std::min(minZ, int64_t(sZ));

			maxX = std::max(maxX, int64_t(sX));
			maxY = std::max(maxY, int64_t(sY));
			maxZ = std::max(maxZ, int64_t(sZ));

		}

		int64_t rangeX = maxX - minX;
		int64_t rangeY = maxY - minY;
		int64_t rangeZ = maxZ - minZ;

		int bitsX = std::log2(rangeX);
		int bitsY = std::log2(rangeY);
		int bitsZ = std::log2(rangeZ);

		auto computeBytes = [](int bits) {
			if (bits <= 8) {
				return 1;
			} else if (bits <= 16) {
				return 2;
			} else if (bits <= 32) {
				return 4;
			}
		};

		int bytesX = computeBytes(bitsX);
		int bytesY = computeBytes(bitsY);
		int bytesZ = computeBytes(bitsZ);

		Buffer X(numPoints * bytesX);
		Buffer Y(numPoints * bytesY);
		Buffer Z(numPoints * bytesZ);

		for (int64_t i = 1; i < numPoints; i++) {
			int64_t offset = i * attributes.bytes;

			int32_t sX, sY, sZ;
			memcpy(&sX, node->points->data_u8 + offset + 0, 4);
			memcpy(&sY, node->points->data_u8 + offset + 4, 4);
			memcpy(&sZ, node->points->data_u8 + offset + 8, 4);

			int32_t oX = sX - minX;
			int32_t oY = sY - minY;
			int32_t oZ = sZ - minZ;

			if (bytesX == 1) {
				X.data_u8[i] = oX;
			} else if (bytesX == 2) {
				X.data_u16[i] = oX;
			} else if (bytesX == 4) {
				X.data_u32[i] = oX;
			}

			if (bytesY == 1) {
				Y.data_u8[i] = oY;
			} else if (bytesY == 2) {
				Y.data_u16[i] = oY;
			} else if (bytesY == 4) {
				Y.data_u32[i] = oY;
			}

			if (bytesZ == 1) {
				Z.data_u8[i] = oZ;
			} else if (bytesZ == 2) {
				Z.data_u16[i] = oZ;
			} else if (bytesZ == 4) {
				Z.data_u32[i] = oZ;
			}

		}

		writeBinaryFile(dir + "/quantized_X.bin", X);
		writeBinaryFile(dir + "/quantized_Y.bin", Y);
		writeBinaryFile(dir + "/quantized_Z.bin", Z);

		int a = 10;

	}

	void writePosition(string dir, Node* node, Attributes attributes) {

		int64_t numPoints = node->numPoints;
		vector<Point> points = getPoints(node, attributes);

		Buffer X(numPoints * 4);
		Buffer Y(numPoints * 4);
		Buffer Z(numPoints * 4);
		Buffer diffX(numPoints * 4);
		Buffer diffY(numPoints * 4);
		Buffer diffZ(numPoints * 4);

		stringstream ssDiffX;
		stringstream ssDiffY;
		stringstream ssDiffZ;

		stringstream ssPoints;

		for (int64_t i = 0; i < numPoints; i++) {
			int64_t offset = i * attributes.bytes;

			Point point = points[i];

			X.data_i32[i] = point.x;
			Y.data_i32[i] = point.y;
			Z.data_i32[i] = point.z;

			int64_t idiffX, idiffY, idiffZ;

			if (i == 0) {
				idiffX = point.x;
				idiffY = point.y;
				idiffZ = point.z;
			} else {
				idiffX = points[i].x - points[i - 1].x;
				idiffZ = points[i].y - points[i - 1].y;
				idiffY = points[i].z - points[i - 1].z;
			}

			diffX.data_i32[i] = idiffX;
			diffY.data_i32[i] = idiffY;
			diffZ.data_i32[i] = idiffZ;

			ssDiffX << idiffX << ", ";
			ssDiffY << idiffY << ", ";
			ssDiffZ << idiffZ << ", ";

			ssPoints << "[" << point.x << ", " << point.y << ", " << point.z << "], ";
			
		}

		writeBinaryFile(dir + "/X.bin", X);
		writeBinaryFile(dir + "/Y.bin", Y);
		writeBinaryFile(dir + "/Z.bin", Z);

		writeBinaryFile(dir + "/diff_X.bin", diffX);
		writeBinaryFile(dir + "/diff_Y.bin", diffY);
		writeBinaryFile(dir + "/diff_Z.bin", diffZ);

		writeFile(dir + "/diffX.txt", ssDiffX.str());
		writeFile(dir + "/diffY.txt", ssDiffY.str());
		writeFile(dir + "/diffZ.txt", ssDiffZ.str());

		writeFile(dir + "/points.txt", ssPoints.str());
	}

	// method to seperate bits from a given integer 3 positions apart
	inline uint64_t splitBy3(unsigned int a) {

		uint64_t x = a & 0x1fffff; // we only look at the first 21 bits
		x = (x | x << 32) & 0x1f00000000ffff; // shift left 32 bits, OR with self, and 00011111000000000000000000000000000000001111111111111111
		x = (x | x << 16) & 0x1f0000ff0000ff; // shift left 32 bits, OR with self, and 00011111000000000000000011111111000000000000000011111111
		x = (x | x << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, and 0001000000001111000000001111000000001111000000001111000000000000
		x = (x | x << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, and 0001000011000011000011000011000011000011000011000011000100000000
		x = (x | x << 2) & 0x1249249249249249;

		return x;
	}

	inline uint64_t mortonEncode_magicbits(unsigned int x, unsigned int y, unsigned int z) {

		uint64_t answer = 0;
		answer |= splitBy3(x) | splitBy3(y) << 1 | splitBy3(z) << 2;

		return answer;
	}

	void writePositionMortonCode(string dir, Node* node, Attributes attributes) {

		int64_t numPoints = node->numPoints;

		vector<Point> points = getPoints(node, attributes);

		auto [min, max] = computeBox(points);

		int64_t rangeX = max.x - min.x;
		int64_t rangeY = max.z - min.z;
		int64_t rangeZ = max.y - min.y;


		vector<uint64_t> mortonCodes;

		for (int64_t i = 0; i < numPoints; i++) {
			int64_t offset = i * attributes.bytes;

			Point point = points[i];

			uint32_t oX = point.x - min.x;
			uint32_t oY = point.z - min.z;
			uint32_t oZ = point.y - min.y;

			uint64_t mortonCode = mortonEncode_magicbits(oX, oY, oZ);

			mortonCodes.push_back(mortonCode);

		}

		sort(mortonCodes.begin(), mortonCodes.end(), [](uint64_t& a, uint64_t& b) {
			return a < b;
		});

		Buffer data(8 * numPoints);
		Buffer dataDiff(8 * numPoints);

		stringstream ss;
		stringstream ssDiff;
		ss << "[";
		ssDiff << "[";

		for (int64_t i = 0; i < numPoints; i++) {
			uint64_t mortonCode = mortonCodes[i];
			data.data_u64[i] = mortonCode;

			uint64_t diff;

			if (i == 0) {
				diff = mortonCode;
			} else {
				diff = mortonCodes[i] - mortonCodes[i - 1];
			}
			dataDiff.data_u64[i] = diff;

			ss << mortonCode << ", ";
			ssDiff << diff << ", ";
		}




		ss << "]";
		ssDiff << "]";

		writeFile(dir + "/mortoncode.txt", ss.str());
		writeFile(dir + "/mortoncode_diff.txt", ssDiff.str());


		writeBinaryFile(dir + "/mortoncode.bin", data);
		writeBinaryFile(dir + "/mortoncode_diff.bin", dataDiff);


		int a = 10;
	}

	void writeIntensity(string dir, Node* node, Attributes attributes) {

		int64_t numPoints = node->numPoints;
		int64_t attributeOffset = attributes.getOffset("intensity");

		//int16_t intensity_origin_u16;
		//memcpy(&intensity_origin_u16, node->points->data_u8 + attributeOffset, 2);
		//int64_t intensity_origin = intensity_origin_u16;

		Buffer I(numPoints * 2);

		memcpy(I.data, node->points->data_u8 + attributeOffset, 2);

		int64_t intensity_previous = I.data_u16[0];
		int64_t intensity_current;

		for (int64_t i = 1; i < numPoints; i++) {
			int64_t offset = i * attributes.bytes;

			int16_t intensity_1_u16;
			memcpy(&intensity_1_u16, node->points->data_u8 + offset + attributeOffset, 2);

			intensity_current = intensity_1_u16;

			int64_t diff = (intensity_current - intensity_previous);
			int64_t sign = diff < 0 ? 0b1000'0000'0000'0000 : 0b0;

			uint16_t diff16 = sign | std::abs(diff);
			//uint16_t diff16 = ((std::abs(diff) << 1) | sign);

			I.data_u16[i] = diff16;

			intensity_previous = intensity_current;
		}

		writeBinaryFile(dir + "/diff_intensity.bin", I);
	}


	void writeNormal(string dir, Node* node, Attributes attributes, Attribute attribute) {
		int attributeOffset = attributes.getOffset(attribute.name);
		int64_t numBytes = attribute.size * node->numPoints;
		Buffer buffer(numBytes);

		uint8_t* source = node->points->data_u8;
		uint8_t* target = buffer.data_u8;

		for (int64_t i = 0; i < node->numPoints; i++) {

			int64_t sourceOffset = i * attributes.bytes + attributeOffset;
			int64_t targetOffset = i * attribute.size;

			memcpy(target + targetOffset, source + sourceOffset, attribute.size);
		}

		string filepath = dir + "/" + attribute.name + ".bin";
		vector<uint8_t> data(target, target + numBytes);
		writeBinaryFile(filepath, data);
	}

	void writeRGB(string dir, Node* node, Attributes attributes, Attribute attribute) {
		int attributeOffset = attributes.getOffset(attribute.name);
		int64_t numPoints = node->numPoints;
		//int64_t numBytes = 4 * numPoints;
		//Buffer buffer(numBytes);

		uint8_t* source = node->points->data_u8;
		//uint8_t* target = buffer.data_u8;

		int64_t dimx = ceil(sqrt(numPoints));
		int64_t dimy = dimx;

		string filepath = dir + "/" + attribute.name + ".ppm";
		ofstream ofs(filepath, ios::out | ios::binary);
		ofs << "P6" << endl << dimx << ' ' << dimy << endl << "255" << endl;


		for (int64_t i = 0; i < numPoints; i++) {

			int64_t sourceOffset = i * attributes.bytes + attributeOffset;

			uint16_t rgb[3];
			memcpy(rgb, source + sourceOffset, 6);

			uint8_t r = rgb[0] > 255 ? rgb[0] / 256 : rgb[0];
			uint8_t g = rgb[1] > 255 ? rgb[1] / 256 : rgb[1];
			uint8_t b = rgb[2] > 255 ? rgb[2] / 256 : rgb[2];

			ofs << r << g << b;
			
			//ofs << (char)(i % 256) << (char)(j % 256) << (char)((i * j) % 256);

			//memcpy(target + targetOffset, source + sourceOffset, attribute.size);
		}
		ofs.close();

	}

	void writeDifference(string dir, Node* node, Attributes attributes, Attribute attribute) {
		
		if (attribute.type == AttributeType::UINT16) {

		}


	}

	void writeNode(Node* node, Attributes attributes){
		
		string dir = "D:/temp/compression/" + node->name;

		fs::create_directories(dir);

		int64_t attributeOffset = 0;
		for (Attribute& attribute : attributes.list) {

			if (attribute.name == "position") {
				//writeNormal(dir, node, attributes, attribute);
				writePosition(dir, node, attributes);
				writePosition2(dir, node, attributes);
				writePositionMortonCode(dir, node, attributes);
			} else if(attribute.name == "intensity"){
				writeIntensity(dir, node, attributes);
			} else if (attribute.name == "rgb") {
				writeRGB(dir, node, attributes, attribute);
			}

			writeNormal(dir, node, attributes, attribute);
			//writeDifference(dir, node, attributes, attribute);

			attributeOffset += attribute.size;
		}


		writeBinaryFile(dir + "/all.bin", *node->points);


	}


}




