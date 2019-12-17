
#pragma once

#include <cstdint>
#include <string>


#include "Vector3.h"

using std::string;

struct Metadata {

	string targetDirectory = "";

	Vector3<double> min;
	Vector3<double> max;
	uint64_t numPoints = 0;

	uint32_t chunkGridSize = 0;

	Metadata() {

	}

};