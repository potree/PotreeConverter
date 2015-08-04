
#ifndef POINT_H
#define POINT_H

#include "Vector3.h"

#include <iostream>

using std::ostream;

class Point{
public:
	Vector3<double> position{0.0};
	Vector3<float> normal{0.0};
	unsigned short intensity = 0;
	unsigned char classification = 0;
	unsigned char r = 255;
	unsigned char g = 255;
	unsigned char b = 255;
	unsigned char returnNumber = 0;
	unsigned char numberOfReturns = 0;
	unsigned short pointSourceID = 0;
	float distance = 0.0;

	Point() = default;

	Point(const Point &other) = default;

	~Point() = default;

	Point(double x, double y, double z, unsigned char r, unsigned char g, unsigned char b){
		position.x = x;
		position.y = y;
		position.z = z;
		this->r = r;
		this->g = g;
		this->b = b;
	}

	Point(double x, double y, double z){
		position.x = x;
		position.y = y;
		position.z = z;
	}

	double distanceTo(const Point &point) {
		return Vector3<double>(point.position.x - position.x, point.position.y - position.y, point.position.z - position.z).length();
	}

	double squaredDistanceTo(const Point &point) {
		return Vector3<double>(point.position.x - position.x, point.position.y - position.y, point.position.z - position.z).squaredLength();
	}

	friend ostream &operator<<( ostream &output,  const Point &value ){ 
		output << "[" << value.position.x << ", " << value.position.y << ", " << value.position.z << "]" ;
		return output;            
	}

};


#endif