
#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Vector3.h"

using std::string;
using std::unordered_map;
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

	// DEBUG
	// 0x70 = 112
	inline static uint8_t defaultvalue = 0x70;

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

		
		memset(this->dataU8, defaultvalue, size);

		this->size = size;
	}

	~Buffer() {
		free(this->data);
	}

	vector<uint8_t> debug_toVector() {
		return vector<uint8_t>(this->dataU8, this->dataU8 + this->size);
	}

};

struct AttributeType {
	int id;
	string name;
	int bytes;

	bool operator==(AttributeType& type) {
		return this->id == type.id;
	}
};

namespace AttributeTypes {
	static const AttributeType undefined   = { 0, "undefined", 0};
	static const AttributeType int8        = { 1, "int8", 1};
	static const AttributeType int16       = { 2, "int16", 2};
	static const AttributeType int32       = { 3, "int32", 4};
	static const AttributeType int64       = { 4, "int64", 8};
	static const AttributeType uint8       = { 5, "uint8", 1};
	static const AttributeType uint16      = { 6, "uint16", 2};
	static const AttributeType uint32      = { 7, "uint32", 4};
	static const AttributeType uint64      = { 8, "uint64", 8};
	static const AttributeType float32     = { 9, "float32", 4};
	static const AttributeType float64     = {10, "float64", 8};

	static const unordered_map<string, AttributeType> map = {
		{"undefined", undefined},
		{"int8", int8},
		{"int16", int16},
		{"int32", int32},
		{"int64", int64},
		{"uint8", uint8},
		{"uint16", uint16},
		{"uint32", uint32},
		{"uint64", uint64},
		{"float32", float32},
		{"float64", float64}
	};

	inline AttributeType fromName(string name) {
		if (map.find(name) == map.end()) {
			cout << "ERROR: attribute type with this name does not exist: " << name << endl;
			cout << __FILE__ << "(" << __LINE__ << ")" << endl;

			exit(123);
		}

		return map.at(name);
	}
}

struct Attribute {

	string name = "undefined";
	string description = "";

	AttributeType type;
	int numElements = 1;
	int bytes = 0;

	Attribute(){

	}

	Attribute(string name, AttributeType type) {
		this->name = name;
		this->type = type;
		this->bytes = type.bytes;
	}

	Attribute(string name, AttributeType type, int numElements) {
		this->name = name;
		this->type = type;
		this->numElements = numElements;
		this->bytes = type.bytes * numElements;
	}

	Attribute(string name, AttributeType type, int offset, int bytes, int numElements) {
		this->name = name;
		this->type = type;
		this->bytes = bytes;
		this->numElements = numElements;
		this->bytes = type.bytes * numElements;
	}

};

struct Attributes {

	vector<Attribute> list;
	int byteSize = 0;

	Attributes() {

	}

	Attributes(vector<Attribute> list) {
		for (auto attribute : list) {
			add(attribute);
		}
	}

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

	Point() {

	}

	Point(double x, double y, double z, int index) {
		this->x = x;
		this->y = y;
		this->z = z;
		this->index = index;
	}

	double squaredDistanceTo(Point& b) {
		double dx = b.x - this->x;
		double dy = b.y - this->y;
		double dz = b.z - this->z;

		double dd = dx * dx + dy * dy + dz * dz;

		return dd;
	};

	double squaredDistanceTo(Vector3<double> b) {
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


