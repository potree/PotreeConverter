
#pragma once

#include "Vector3.h"


struct ScaleOffset {
	Vector3 scale;
	Vector3 offset;
};


ScaleOffset computeScaleOffset(Vector3 min, Vector3 max, Vector3 targetScale) {

	Vector3 center = (min + max) / 2.0;
	
	// using the center as the origin would be the "right" choice but 
	// it would lead to negative integer coordinates.
	// since the Potree 1.7 release mistakenly interprets the coordinates as uint values,
	// we can't do that and we use 0/0/0 as the bounding box minimum as the origin instead.
	//Vector3 offset = center;

	Vector3 offset = min;
	Vector3 scale = targetScale;
	Vector3 size = max - min;

	// we can only use 31 bits because of the int/uint mistake in Potree 1.7
	// And we only use 30 bits to be on the safe sie.
	double min_scale_x = size.x / pow(2.0, 30.0);
	double min_scale_y = size.y / pow(2.0, 30.0);
	double min_scale_z = size.z / pow(2.0, 30.0);

	scale.x = std::max(scale.x, min_scale_x);
	scale.y = std::max(scale.y, min_scale_y);
	scale.z = std::max(scale.z, min_scale_z);

	ScaleOffset scaleOffset;
	scaleOffset.scale = scale;
	scaleOffset.offset = offset;

	return scaleOffset;
}

