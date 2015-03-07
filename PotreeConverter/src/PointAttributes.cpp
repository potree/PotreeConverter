
#include "PointAttributes.hpp"

const PointAttribute PointAttribute::POSITION_CARTESIAN = PointAttribute(0, "POSITION_CARTESIAN", 3, 3*sizeof(float));
const PointAttribute PointAttribute::COLOR_PACKED = PointAttribute(1, "COLOR_PACKED", 4, 4*sizeof(unsigned char));
const PointAttribute PointAttribute::INTENSITY = PointAttribute(2, "INTENSITY", 1, sizeof(unsigned short));
const PointAttribute PointAttribute::CLASSIFICATION = PointAttribute(3, "CLASSIFICATION", 1, sizeof(unsigned char));

bool operator==(const PointAttribute& lhs, const PointAttribute& rhs){ 
	return lhs.ordinal == rhs.ordinal;
}