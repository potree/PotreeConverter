
#ifndef POINT_H
#define POINT_H

#include "Vector3.h"

#include <iostream>

using std::ostream;

class Point{
public:

	Vector3<double> position;
#if 0
	double x;
	double y;
	double z;
#endif
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
		position( ),
#if 0
		x( 0 ),
		y( 0 ),
		z( 0 ),
#endif
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
		position( p_x, p_y, p_z ),
#if 0
		x( p_x ),
		y( p_y ),
		z( p_z ),
#endif
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
		position( p_x, p_y, p_z ),
#if 0
		x( p_x ),
		y( p_y ),
		z( p_z ),
#endif
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
		: position(other.position),
#if 0
		: x(other.x), y(other.y), z(other.z), 
#endif
		nx(other.nx), ny(other.ny), nz(other.nz),
		intensity(other.intensity), classification(other.classification), 
		r(other.r), g(other.g), b(other.b), 
		returnNumber(other.returnNumber), numberOfReturns(other.numberOfReturns), 
		pointSourceID(other.pointSourceID)
	{
	}

	~Point() = default;

	double distanceTo(const Point &point) const {
		return position.distanceTo( point.position );
#if 0
		return Vector3<double>(point.x - x, point.y - y, point.z - z).length();
#endif
	}

	double squaredDistanceTo(const Point &point) const{
		return position.squaredDistanceTo( point.position );
#if 0
		return Vector3<double>(point.x - x, point.y - y, point.z - z).squaredLength();
#endif
	}
#if 0
	Vector3<double> position(){
		return Vector3<double>(x, y, z);
	}
#endif
	friend ostream &operator<<( ostream &output,  const Point &value ){ 
		return output << value.position;
#if 0
		output << "[" << value.x << ", " << value.y << ", " << value.z << "]" ;
		return output;            
#endif
	}

};


#endif
