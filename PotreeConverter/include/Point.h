
#ifndef POINT_H
#define POINT_H

#include "Vector3.h"

#include <iostream>

using std::ostream;

class Point{
public:

	Vector3<double> position;
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
	}

	double squaredDistanceTo(const Point &point) const{
		return position.squaredDistanceTo( point.position );
	}
	friend ostream &operator<<( ostream &output,  const Point &value ){ 
		return output << value.position;
	}

};


#endif
