
#pragma once

#include "math.h"
#include "Points.h"

int computeChildIndex(BoundingBox box, Point point) {
	int childIndex = 0;

	double nx = (point.x - box.min.x) / (box.max.x - box.min.x);
	double ny = (point.y - box.min.y) / (box.max.x - box.min.x);
	double nz = (point.z - box.min.z) / (box.max.x - box.min.x);

	if (nx > 0.5) {
		childIndex = childIndex | 0b100;
	} 

	if (ny > 0.5) {
		childIndex = childIndex | 0b010;
	} 

	if (nz > 0.5) {
		childIndex = childIndex | 0b001;
	}

	return childIndex;
}

BoundingBox childBoundingBoxOf(BoundingBox in, int index) {
	BoundingBox box;
	Vector3<double> size = in.size();
	Vector3<double> center = in.min + (size * 0.5);

	if ((index & 0b100) == 0) {
		box.min.x = in.min.x;
		box.max.x = center.x;
	} else {
		box.min.x = center.x;
		box.max.x = in.max.x;
	}

	if ((index & 0b010) == 0) {
		box.min.y = in.min.y;
		box.max.y = center.y;
	} else {
		box.min.y = center.y;
		box.max.y = in.max.y;
	}

	if ((index & 0b001) == 0) {
		box.min.z = in.min.z;
		box.max.z = center.z;
	} else {
		box.min.z = center.z;
		box.max.z = in.max.z;
	}

	return box;
}