
#ifndef VECTOR3_H
#define VECTOR3_H

#include <math.h>
#include <iostream>

using std::ostream;
using std::max;

class Vector3{

public:
	double x,y,z;

	Vector3(){
		x = 0;
		y = 0;
		z = 0;
	}

	Vector3(double x, double y, double z){
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vector3(double value){
		this->x = value;
		this->y = value;
		this->z = value;
	}

	double length(){
		return sqrt(x*x + y*y + z*z);
	}

	double squaredLength(){
		return x*x + y*y + z*z;
	}

	double distanceTo(Vector3 p){
		return ((*this) - p).length();
	}

	double maxValue(){
		return max(x, max(y,z));
	}

	Vector3 operator-(const Vector3& right) const {
		return Vector3(x - right.x, y - right.y, z - right.z);
	}

	Vector3 operator+(const Vector3& right) const {
		return Vector3(x + right.x, y + right.y, z + right.z);
	}

	Vector3 operator/(const double &a) const{
		return Vector3(x / a, y / a, z / a);
	}

	friend ostream &operator<<( ostream &output,  const Vector3 &value ){ 
		output << "[" << value.x << ", " << value.y << ", " << value.z << "]" ;
		return output;            
	}
};

#endif