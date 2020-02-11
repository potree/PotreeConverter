
#pragma once

#include "Vector3.h"


class BoundingBox {
public:
	Vector3<double> min;
	Vector3<double> max;

	BoundingBox() {
		this->min = { Infinity,Infinity,Infinity};
		this->max = { -Infinity,-Infinity,-Infinity };
	}

	BoundingBox(Vector3<double> min, Vector3<double> max) {
		this->min = min;
		this->max = max;
	}

	Vector3<double> size() {
		return max - min;
	}
};

