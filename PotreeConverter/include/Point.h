
#ifndef POINT_H
#define POINT_H

#include "Vector3.h"

#include <iostream>

using std::ostream;

class Point{
public:
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	float nx = 0.0;
	float ny = 0.0;
	float nz = 0.0;
	unsigned short intensity = 0;
	unsigned char classification = 0;
	unsigned char r = 255;
	unsigned char g = 255;
	unsigned char b = 255;
	unsigned char a = 255;
	unsigned char returnNumber = 0;
	unsigned char numberOfReturns = 0;
	unsigned short pointSourceID = 0;
	float distance = 0.0;

	Point() = default;

	~Point() = default;

	Point(double x, double y, double z, unsigned char r, unsigned char g, unsigned char b){
		this->x = x;
		this->y = y;
		this->z = z;
		this->r = r;
		this->g = g;
		this->b = b;
	}

	Point(double x, double y, double z){
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Point(const Point &other)
		: x(other.x), y(other.y), z(other.z), 
		r(other.r), g(other.g), b(other.b),
		nx(other.nx), ny(other.ny), nz(other.nz),
		intensity(other.intensity), 
		classification(other.classification), 
		returnNumber(other.returnNumber), 
		numberOfReturns(other.numberOfReturns), 
		pointSourceID(other.pointSourceID)		
	{

	}

	double distanceTo(const Point &point) {
		return Vector3<double>(point.x - x, point.y - y, point.z - z).length();
	}

	double squaredDistanceTo(const Point &point) {
		return Vector3<double>(point.x - x, point.y - y, point.z - z).squaredLength();
	}

	Vector3<double> position(){
		return Vector3<double>(x, y, z);
	}

	friend ostream &operator<<( ostream &output,  const Point &value ){ 
		output << "[" << value.x << ", " << value.y << ", " << value.z << "]" ;
		return output;            
	}

};


#endif