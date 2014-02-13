
#ifndef VECTOR3_H
#define VECTOR3_H

#include <math.h>
#include <iostream>

using std::ostream;

class Vector3{

public:
	float x,y,z;

	Vector3(){
		x = 0;
		y = 0;
		z = 0;
	}

	Vector3(float x, float y, float z){
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vector3(float value){
		this->x = value;
		this->y = value;
		this->z = value;
	}

	float length(){
		return sqrt(x*x + y*y + z*z);
	}

	float distanceTo(Vector3 p){
		return ((*this) - p).length();
	}

	Vector3 operator-(Vector3& right) const {
		return Vector3(x - right.x, y - right.y, z - right.z);
	}

	Vector3 operator+(Vector3& right) const {
		return Vector3(x + right.x, y + right.y, z + right.z);
	}

	Vector3 operator/(const float &a) const{
		return Vector3(x / a, y / a, z / a);
	}

	friend ostream &operator<<( ostream &output,  const Vector3 &value ){ 
		output << "[" << value.x << ", " << value.y << ", " << value.z << "]" ;
		return output;            
	}
};

#endif