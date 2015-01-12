

#ifndef POINT_ATTRIBUTES_H
#define POINT_ATTRIBUTES_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class PointAttribute{
public:
	static const PointAttribute POSITION_CARTESIAN;
	static const PointAttribute COLOR_PACKED;

	int ordinal;
	string name;
	int numElements;
	int byteSize;

	PointAttribute(int ordinal, string name, int numElements, int byteSize){
		this->ordinal = ordinal;
		this->name = name;
		this->numElements = numElements;
		this->byteSize = byteSize;
	}

	//bool operator==(const PointAttribute &other){
	//	return ordinal == other.ordinal;
	//}

};

bool operator==(const PointAttribute& lhs, const PointAttribute& rhs);

//class PointAttributeDef{
//public:
//	string name;
//	int numElements;
//	int byteSize;
//
//	PointAttributeDef(string name, int numElements, int byteSize){
//		this->name = name;
//		this->numElements = numElements;
//		this->byteSize = byteSize;
//	}
//};
//
//namespace PointAttribute{
//	PointAttributeDef POSITION_CARTESIAN("POSITION_CARTESIAN", 3, 3*sizeof(float));
//	PointAttributeDef COLOR_PACKED("COLOR_PACKED", 4, 4*sizeof(unsigned char));
//}

//struct PointAttribute{
//	const PointAttributeDef POSITION_CARTESIAN("POSITION_CARTESIAN", 3, 3*sizeof(float));
//	const PointAttributeDef COLOR_PACKED("COLOR_PACKED", 4, 4*sizeof(unsigned char));
//};


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






#endif