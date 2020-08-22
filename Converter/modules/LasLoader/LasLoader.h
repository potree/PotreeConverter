
#pragma once

#include "unsuck/unsuck.hpp"
#include "Vector3.h"
#include "Attributes.h"


struct LasTypeInfo {
	AttributeType type = AttributeType::UNDEFINED;
	int numElements = 0;
};

struct VLR {
	char userID[16];
	uint16_t recordID = 0;
	uint16_t recordLengthAfterHeader = 0;
	char description[32];

	vector<uint8_t> data;
};

struct LasHeader {
	Vector3 min;
	Vector3 max;
	Vector3 scale;
	Vector3 offset;

	int64_t numPoints = 0;

	int pointDataFormat = -1;

	vector<VLR> vlrs;
};


LasTypeInfo lasTypeInfo(int typeID);

LasHeader loadLasHeader(string path);



