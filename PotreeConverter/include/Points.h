
#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::vector;

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

struct Attribute {

	string name = "undefined";
	int64_t byteOffset = 0;
	int64_t bytes = 0;

	Attribute() {

	}

};

struct Points {

	int64_t count = 0;
	vector<Attribute> attributes;
	vector<Buffer*> data;

};