

#ifndef AABB_H
#define AABB_H

#include "Vector3.h"
#include "Point.h"

#include <math.h>
#include <algorithm>

using std::min;
using std::max;
using std::endl;

class AABB{

public:
	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;

	AABB(){
		min = Vector3<double>(std::numeric_limits<float>::max());
		max = Vector3<double>(-std::numeric_limits<float>::max());
		size = Vector3<double>(std::numeric_limits<float>::max());
	}

	AABB(Vector3<double> min, Vector3<double> max){
		this->min = min;
		this->max = max;
		size = max-min;
	}

	bool isInside(const Point &p){
		if(min.x <= p.position.x && p.position.x <= max.x){
			if(min.y <= p.position.y && p.position.y <= max.y){
				if(min.z <= p.position.z && p.position.z <= max.z){
					return true;
				}
			}
		}

		return false;
	}

	void update(Vector3<double> &point){
		min.x = std::min(min.x, point.x);
		min.y = std::min(min.y, point.y);
		min.z = std::min(min.z, point.z);

		max.x = std::max(max.x, point.x);
		max.y = std::max(max.y, point.y);
		max.z = std::max(max.z, point.z);

		size = max-min;
	}

	void makeCubic(){
		double maxlength = size.maxValue();
		max.x = min.x + maxlength;
		max.y = min.y + maxlength;
		max.z = min.z + maxlength;
		size = max - min;


		//if(abs(min.x) > max.x){
		//	min.x = max.x - maxlength;
		//}else{
		//	max.x = min.x + maxlength;
		//}
		//
		//if(abs(min.y) > max.y){
		//	min.y = max.y - maxlength;
		//}else{
		//	max.y = min.y + maxlength;
		//}
		//
		//if(abs(min.z) > max.z){
		//	min.z = max.z - maxlength;
		//}else{
		//	max.z = min.z + maxlength;
		//}
	}

	friend ostream &operator<<( ostream &output,  const AABB &value ){ 
		output << "min: " << value.min << endl;
		output << "max: " << value.max << endl;
		output << "size: " << value.size << endl;
		return output;            
	}

};



#endif
