
#pragma once

#include "convmath.h"
#include "Points.h"
#include "stuff.h"


inline int childIndexOf(Vector3<double>& min, Vector3<double>& max, Point& point) {
	int childIndex = 0;

	double nx = (point.x - min.x) / (max.x - min.x);
	double ny = (point.y - min.y) / (max.x - min.x);
	double nz = (point.z - min.z) / (max.x - min.x);

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

inline int computeChildIndex(BoundingBox box, Point point) {
	return childIndexOf(box.min, box.max, point);
}

inline BoundingBox childBoundingBoxOf(Vector3<double> min, Vector3<double> max, int index) {
	BoundingBox box;
	auto size = max - min;
	Vector3<double> center = min + (size * 0.5);

	if ((index & 0b100) == 0) {
		box.min.x = min.x;
		box.max.x = center.x;
	} else {
		box.min.x = center.x;
		box.max.x = max.x;
	}

	if ((index & 0b010) == 0) {
		box.min.y = min.y;
		box.max.y = center.y;
	} else {
		box.min.y = center.y;
		box.max.y = max.y;
	}

	if ((index & 0b001) == 0) {
		box.min.z = min.z;
		box.max.z = center.z;
	} else {
		box.min.z = center.z;
		box.max.z = max.z;
	}

	return box;
}

inline BoundingBox childBoundingBoxOf(BoundingBox in, int index) {
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

inline vector<Point> loadPoints(string file) {
	auto buffer = readBinaryFile(file);

	int64_t numPoints = buffer.size() / 28;

	vector<Point> points;
	points.reserve(numPoints);

	for (int i = 0; i < numPoints; i++) {

		double x = reinterpret_cast<double*>(buffer.data() + (28 * i + 0))[0];
		double y = reinterpret_cast<double*>(buffer.data() + (28 * i + 0))[1];
		double z = reinterpret_cast<double*>(buffer.data() + (28 * i + 0))[2];

		Point point;
		point.x = x;
		point.y = y;
		point.z = z;
		point.index = i;

		points.push_back(point);
	}

	return points;

}