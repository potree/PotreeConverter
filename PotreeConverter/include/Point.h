
#ifndef POINT_H
#define POINT_H

#include "Vector3.h"

#include <iostream>

using std::ostream;

class Point{
public:
	static long long count;

	float x;
	float y;
	float z;
	unsigned short intensity;
	unsigned char classification;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;

	Point(){
		this->x = 0;
		this->y = 0;
		this->z = 0;
		this->intensity = 0;
		this->classification = 0;
		this->r = 0;
		this->g = 0;
		this->b = 0;
		this->a = 255;
		

		count++;
	}

	Point(float x, float y, float z, unsigned char r, unsigned char g, unsigned char b){
		this->x = x;
		this->y = y;
		this->z = z;
		this->intensity = 0;
		this->classification = 0;
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = 255;

		count++;
	}

	Point(float x, float y, float z){
		this->x = x;
		this->y = y;
		this->z = z;
		this->intensity = 0;
		this->classification = 0;
		this->r = 255;
		this->g = 255;
		this->b = 255;
		this->a = 255;

		count++;
	}

	Point(const Point &other)
		: x(other.x), y(other.y), z(other.z), intensity(other.intensity), classification(other.classification), r(other.r), g(other.g), b(other.b)
	{
		count++;
	}

	~Point(){
		count--;
	}

	float distanceTo(const Point &point) {
		return Vector3<double>(point.x - x, point.y - y, point.z - z).length();
	}

	float squaredDistanceTo(const Point &point) {
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