
#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>

#include "Vector3.h"
#include "unsuck/unsuck.hpp"

using std::string;
using std::unordered_map;
using std::vector;

enum class AttributeType {
	INT8 = 0,
	INT16 = 1,
	INT32 = 2,
	INT64 = 3,

	UINT8 = 10,
	UINT16 = 11,
	UINT32 = 12,
	UINT64 = 13,

	FLOAT = 20,
	DOUBLE = 21,

	UNDEFINED = 123456,
};

inline int getAttributeTypeSize(AttributeType type) {
	unordered_map<AttributeType, int> mapping = {
		{AttributeType::UNDEFINED, 0},
		{AttributeType::UINT8, 1},
		{AttributeType::UINT16, 2},
		{AttributeType::UINT32, 4},
		{AttributeType::UINT64, 8},
		{AttributeType::INT8, 1},
		{AttributeType::INT16, 2},
		{AttributeType::INT32, 4},
		{AttributeType::INT64, 8},
		{AttributeType::FLOAT, 4},
		{AttributeType::DOUBLE, 8},
	};

	return mapping[type];
}

inline string getAttributeTypename(AttributeType type) {

	if(type == AttributeType::INT8){
		return "int8";
	}else if(type == AttributeType::INT16){
		return "int16";
	}else if(type == AttributeType::INT32){
		return "int32";
	}else if(type == AttributeType::INT64){
		return "int64";
	}else if(type == AttributeType::UINT8){
		return "uint8";
	}else if(type == AttributeType::UINT16){
		return "uint16";
	}else if(type == AttributeType::UINT32){
		return "uint32";
	}else if(type == AttributeType::UINT64){
		return "uint64";
	}else if(type == AttributeType::FLOAT){
		return "float";
	}else if(type == AttributeType::DOUBLE){
		return "double";
	}else if(type == AttributeType::UNDEFINED){
		return "undefined";
	}else {
		return "error";
	}
}

inline AttributeType typenameToType(string name) {
	if(name == "int8"){
		return AttributeType::INT8;
	}else if(name == "int16"){
		return AttributeType::INT16;
	}else if(name == "int32"){
		return AttributeType::INT32;
	}else if(name == "int64"){
		return AttributeType::INT64;
	}else if(name == "uint8"){
		return AttributeType::UINT8;
	}else if(name == "uint16"){
		return AttributeType::UINT16;
	}else if(name == "uint32"){
		return AttributeType::UINT32;
	}else if(name == "uint64"){
		return AttributeType::UINT64;
	}else if(name == "float"){
		return AttributeType::FLOAT;
	}else if(name == "double"){
		return AttributeType::DOUBLE;
	}else if(name == "undefined"){
		return AttributeType::UNDEFINED;
	}else{
		std::cout << "ERROR: unkown AttributeType: '" << name << "'" << std::endl;
		exit(123);
	}
}

struct Attribute {
	string name = "";
	string description = "";
	int size = 0;
	int numElements = 0;
	int elementSize = 0;
	AttributeType type = AttributeType::UNDEFINED;

	// TODO: should be type-dependent, not always double. won't work properly with 64 bit integers
	Vector3 min = {Infinity, Infinity, Infinity};
	Vector3 max = {-Infinity, -Infinity, -Infinity};

	Attribute() {

	}

	Attribute(string name, int size, int numElements, int elementSize, AttributeType type) {
		this->name = name;
		this->size = size;
		this->numElements = numElements;
		this->elementSize = elementSize;
		this->type = type;
	}
};

struct Attributes {

	vector<Attribute> list;
	int bytes = 0;

	Vector3 posScale = Vector3{ 1.0, 1.0, 1.0 };
	Vector3 posOffset = Vector3{ 0.0, 0.0, 0.0 };

	Attributes() {

	}

	Attributes(vector<Attribute> attributes) {
		this->list = attributes;

		for (auto& attribute : attributes) {
			bytes += attribute.size;
		}
	}

	int getOffset(string name) {
		int offset = 0;

		for (auto& attribute : list) {

			if (attribute.name == name) {
				return offset;
			}

			offset += attribute.size;
		}

		return -1;
	}

	Attribute* get(string name) {
		for (auto& attribute : list) {
			if (attribute.name == name) {
				return &attribute;
			}
		}
		
		return nullptr;
	}

};



