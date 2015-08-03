
#ifndef POINT_H
#define POINT_H

#include "Vector3.h"

#include <iostream>

using std::ostream;

class Point{
public:

	double x;
	double y;
	double z;
	float nx;
	float ny;
	float nz;
	unsigned short intensity;
	unsigned char classification;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
	unsigned char returnNumber;
	unsigned char numberOfReturns;
	unsigned short pointSourceID;


	Point() :
		x( 0 ),
		y( 0 ),
		z( 0 ),
		nx( 0 ),
		ny( 0 ),
		nz( 0 ),
		intensity( 0 ),
		classification( 0 ),
		r( 255 ),
		g( 255 ),
		b( 255 ),
		a( 255 ),
		returnNumber( 0 ),
		numberOfReturns( 0 ),
		pointSourceID( 0 ) {

	}

	Point(double p_x, double p_y, double p_z, unsigned char p_r, unsigned char p_g, unsigned char p_b) :
		x( p_x ),
		y( p_y ),
		z( p_z ),
		nx( 0 ),
		ny( 0 ),
		nz( 0 ),
		intensity( 0 ),
		classification( 0 ),
		r( p_r ),
		g( p_g ),
		b( p_b ),
		a( 255 ),
		returnNumber( 0 ),
		numberOfReturns( 0 ),
		pointSourceID( 0 ) {

	}

	Point(double p_x, double p_y, double p_z) :
		x( p_x ),
		y( p_y ),
		z( p_z ),
		nx( 0 ),
		ny( 0 ),
		nz( 0 ),
		intensity( 0 ),
		classification( 0 ),
		r( 255 ),
		g( 255 ),
		b( 255 ),
		a( 255 ),
		returnNumber( 0 ),
		numberOfReturns( 0 ),
		pointSourceID( 0 ) {

	}

	Point(const Point &other)
		: x(other.x), y(other.y), z(other.z), 
		nx(other.nx), ny(other.ny), nz(other.nz),
		intensity(other.intensity), classification(other.classification), 
		r(other.r), g(other.g), b(other.b), 
		returnNumber(other.returnNumber), numberOfReturns(other.numberOfReturns), 
		pointSourceID(other.pointSourceID)
	{
	}

	~Point() = default;

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
