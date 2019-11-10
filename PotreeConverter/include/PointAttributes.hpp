

#ifndef POINT_ATTRIBUTES_H
#define POINT_ATTRIBUTES_H

#include <string>
#include <vector>
#include <unordered_map>

using std::string;
using std::vector;
using std::unordered_map;

namespace Potree{

	#define ATTRIBUTE_TYPE_INT8 "int8"
	#define ATTRIBUTE_TYPE_INT16 "int16"
	#define ATTRIBUTE_TYPE_INT32 "int32"
	#define ATTRIBUTE_TYPE_INT64 "int64"
	#define ATTRIBUTE_TYPE_UINT8 "uint8"
	#define ATTRIBUTE_TYPE_UINT16 "uint16"
	#define ATTRIBUTE_TYPE_UINT32 "uint32"
	#define ATTRIBUTE_TYPE_UINT64 "uint64"
	#define ATTRIBUTE_TYPE_FLOAT "float"
	#define ATTRIBUTE_TYPE_DOUBLE "double"

	const unordered_map<string, int> attributeTypeSize = {
		{ATTRIBUTE_TYPE_INT8, 1},
		{ATTRIBUTE_TYPE_INT16, 2},
		{ATTRIBUTE_TYPE_INT32, 4},
		{ATTRIBUTE_TYPE_INT64, 8},
		{ATTRIBUTE_TYPE_UINT8, 1},
		{ATTRIBUTE_TYPE_UINT16, 2},
		{ATTRIBUTE_TYPE_UINT32, 4},
		{ATTRIBUTE_TYPE_UINT64, 8},
		{ATTRIBUTE_TYPE_FLOAT, 4},
		{ATTRIBUTE_TYPE_DOUBLE, 8}
	};

class PointAttribute{
public:
	static const PointAttribute POSITION_CARTESIAN;
	static const PointAttribute COLOR_PACKED;
	static const PointAttribute INTENSITY;
	static const PointAttribute CLASSIFICATION;
	static const PointAttribute RETURN_NUMBER;
	static const PointAttribute NUMBER_OF_RETURNS;
	static const PointAttribute SOURCE_ID;
	static const PointAttribute GPS_TIME;
	static const PointAttribute NORMAL_SPHEREMAPPED;
	static const PointAttribute NORMAL_OCT16;
	static const PointAttribute NORMAL;

	int ordinal;
	string name;
	string description;
	string type;
	int numElements;
	int byteSize;

	PointAttribute(int ordinal, string name, string type, int numElements, int byteSize){
		this->ordinal = ordinal;
		this->name = name;
		this->type = type;
		this->numElements = numElements;
		this->byteSize = byteSize;
	}

	static PointAttribute fromString(string name);

};

bool operator==(const PointAttribute& lhs, const PointAttribute& rhs);


class PointAttributes{
public:
	vector<PointAttribute> attributes;
	int byteSize;

	PointAttributes(){
		byteSize = 0;
	}

	void add(PointAttribute attribute){
		attributes.push_back(attribute);
		byteSize += attribute.byteSize;
	}

	int size(){
		return (int)attributes.size();
	}

	PointAttribute& operator[](int i) { 
		return attributes[i]; 
	}


};


}




#endif