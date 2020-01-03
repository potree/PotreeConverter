
#pragma once

#include "Points.h"
#include "Vector3.h"

struct LASHeader {

	int headerSize = 375;
	uint64_t numPoints = 0;
	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> scale;

};


void writeLAS(string path, LASHeader header, vector<Point> points);
void writeLAS(string path, LASHeader header, vector<Point> sample, Points* points);