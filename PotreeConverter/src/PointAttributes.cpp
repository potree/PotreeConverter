
#include "PointAttributes.hpp"

namespace Potree{

const PointAttribute PointAttribute::POSITION_CARTESIAN		= PointAttribute(0, "POSITION_CARTESIAN",	3, 12);
const PointAttribute PointAttribute::COLOR_PACKED			= PointAttribute(1, "COLOR_PACKED",			4, 4);
const PointAttribute PointAttribute::INTENSITY				= PointAttribute(2, "INTENSITY",			1, 2);
const PointAttribute PointAttribute::CLASSIFICATION			= PointAttribute(3, "CLASSIFICATION",		1, 1);
const PointAttribute PointAttribute::NORMAL_SPHEREMAPPED	= PointAttribute(4, "NORMAL_SPHEREMAPPED",	2, 2);
const PointAttribute PointAttribute::NORMAL_OCT16			= PointAttribute(5, "NORMAL_OCT16",			2, 2);
const PointAttribute PointAttribute::NORMAL					= PointAttribute(6, "NORMAL",				3, 12);

PointAttribute PointAttribute::fromString(string name){
	if(name == "POSITION_CARTESIAN"){
		return PointAttribute::POSITION_CARTESIAN;
	}else if(name == "COLOR_PACKED"){
		return PointAttribute::COLOR_PACKED;
	}else if(name == "INTENSITY"){
		return PointAttribute::INTENSITY;
	}else if(name == "CLASSIFICATION"){
		return PointAttribute::CLASSIFICATION;
	}else if(name == "NORMAL_SPHEREMAPPED"){
		return PointAttribute::NORMAL_SPHEREMAPPED;
	}else if(name == "NORMAL_OCT16"){
		return PointAttribute::NORMAL_OCT16;
	}else if(name == "NORMAL"){
		return PointAttribute::NORMAL;
	}
}

bool operator==(const PointAttribute& lhs, const PointAttribute& rhs){ 
	return lhs.ordinal == rhs.ordinal;
}

}