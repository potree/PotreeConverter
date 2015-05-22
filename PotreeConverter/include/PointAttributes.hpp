

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
	static const PointAttribute INTENSITY;
	static const PointAttribute CLASSIFICATION;
	static const PointAttribute NORMAL_SPHEREMAPPED;

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






#endif