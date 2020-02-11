
#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include "Vector3.h"

using std::string;
using std::vector;
using std::shared_ptr;
using std::cout;
using std::endl;

struct Buffer {
	void* data = nullptr;
	uint8_t* dataU8 = nullptr;
	uint16_t* dataU16 = nullptr;
	uint32_t* dataU32 = nullptr;
	int8_t* dataI8 = nullptr;
	int16_t* dataI16 = nullptr;
	int32_t* dataI32 = nullptr;
	float* dataF = nullptr;
	double* dataD = nullptr;
	char* dataChar = nullptr;

	uint64_t size = 0;

	Buffer(uint64_t size) {

		this->data = malloc(size);

		this->dataU8 = reinterpret_cast<uint8_t*>(this->data);
		this->dataU16 = reinterpret_cast<uint16_t*>(this->data);
		this->dataU32 = reinterpret_cast<uint32_t*>(this->data);
		this->dataI8 = reinterpret_cast<int8_t*>(this->data);
		this->dataI16 = reinterpret_cast<int16_t*>(this->data);
		this->dataI32 = reinterpret_cast<int32_t*>(this->data);
		this->dataF = reinterpret_cast<float*>(this->data);
		this->dataD = reinterpret_cast<double*>(this->data);
		this->dataChar = reinterpret_cast<char*>(this->data);

		this->size = size;
	}

	~Buffer() {
		free(this->data);
	}

};

enum class AttributeType {
	undefined,
	int8,
	int16,
	int32,
	int64,
	uint8,
	uint16,
	uint32,
	uint64,
	float32, 
	float64,
};

struct Attribute {

	string name = "undefined";
	string description = "";

	AttributeType type;
	int numElements = 1;

	int64_t byteOffset = 0;
	int64_t bytes = 0;

	//Vector3<double> scale = {0.0, 0.0, 0.0};
	//Vector3<double> offset = {0.0, 0.0, 0.0};

	Attribute(string name, AttributeType type) {
		this->name = name;
		this->type = type;
	}

};

struct Attributes {

	vector<Attribute> list;
	int byteSize = 0;

	void add(Attribute attribute) {
		list.push_back(attribute);
		byteSize += attribute.bytes;
	}

};

struct Point {

	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	uint64_t index = 0;

	double squaredDistanceTo(Point& b) {
		double dx = b.x - this->x;
		double dy = b.y - this->y;
		double dz = b.z - this->z;

		double dd = dx * dx + dy * dy + dz * dz;

		return dd;
	};
};


struct Points {
	vector<Point> points;
	
	Attributes attributes;
	shared_ptr<Buffer> attributeBuffer;

	Points() {
		//cout << "create points" << endl;
	}

	~Points() {
		//cout << "delete points" << endl;
	}

};


