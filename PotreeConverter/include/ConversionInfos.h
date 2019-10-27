
#pragma once

#include <vector>
#include <string>

#include "AABB.h"
#include "PointAttributes.hpp"
#include "Point.h"

namespace Potree {

	using std::vector;
	using std::string;

	struct ConversionInfos {
		vector<string> sources;
		AABB aabb;
		uint64_t numPoints = 0;
		PointAttributes attributes;
		double scale = 0.001;

		vector<AABB> aabbs;
		vector<uint64_t> pointCounts;
		vector<string> sourceFilenames;
	};

	struct Batch {
		string filename = "";
		string path = "";
		int level = 0;
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
		uint32_t index = 0;
		vector<Point> points;
		uint64_t numPoints = 0;
	};

}