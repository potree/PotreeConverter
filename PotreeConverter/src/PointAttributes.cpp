
#include "PointAttributes.hpp"
#include "PotreeException.h"

namespace Potree{

const PointAttribute PointAttribute::POSITION_CARTESIAN		= PointAttribute(0, "POSITION_CARTESIAN",	ATTRIBUTE_TYPE_INT32, 3, 12);
const PointAttribute PointAttribute::COLOR_PACKED			= PointAttribute(1, "COLOR_PACKED",			ATTRIBUTE_TYPE_UINT8, 4, 4);
const PointAttribute PointAttribute::INTENSITY				= PointAttribute(2, "INTENSITY",			ATTRIBUTE_TYPE_UINT16, 1, 2);
const PointAttribute PointAttribute::CLASSIFICATION			= PointAttribute(3, "CLASSIFICATION",		ATTRIBUTE_TYPE_UINT8, 1, 1);
const PointAttribute PointAttribute::RETURN_NUMBER			= PointAttribute(4, "RETURN_NUMBER",		ATTRIBUTE_TYPE_UINT8, 1, 1);
const PointAttribute PointAttribute::NUMBER_OF_RETURNS		= PointAttribute(5, "NUMBER_OF_RETURNS",	ATTRIBUTE_TYPE_UINT8, 1, 1);
const PointAttribute PointAttribute::SOURCE_ID				= PointAttribute(6, "SOURCE_ID",			ATTRIBUTE_TYPE_UINT16, 1, 2);
const PointAttribute PointAttribute::GPS_TIME				= PointAttribute(7, "GPS_TIME",				ATTRIBUTE_TYPE_DOUBLE, 1, 8);
const PointAttribute PointAttribute::NORMAL_SPHEREMAPPED	= PointAttribute(8, "NORMAL_SPHEREMAPPED",	ATTRIBUTE_TYPE_INT8, 2, 2);
const PointAttribute PointAttribute::NORMAL_OCT16			= PointAttribute(9, "NORMAL_OCT16",			ATTRIBUTE_TYPE_INT8, 2, 2);
const PointAttribute PointAttribute::NORMAL					= PointAttribute(10, "NORMAL",				ATTRIBUTE_TYPE_FLOAT, 3, 12);

PointAttribute PointAttribute::fromString(string name){
	if(name == "POSITION_CARTESIAN"){
		return PointAttribute::POSITION_CARTESIAN;
	}else if(name == "COLOR_PACKED"){
		return PointAttribute::COLOR_PACKED;
	}else if(name == "INTENSITY"){
		return PointAttribute::INTENSITY;
	}else if(name == "CLASSIFICATION"){
		return PointAttribute::CLASSIFICATION;
	} else if (name == "RETURN_NUMBER") {
		return PointAttribute::RETURN_NUMBER;
	} else if (name == "NUMBER_OF_RETURNS") {
		return PointAttribute::NUMBER_OF_RETURNS;
	} else if (name == "SOURCE_ID") {
		return PointAttribute::SOURCE_ID;
	} else if(name == "GPS_TIME"){
		return PointAttribute::GPS_TIME;
	}else if(name == "NORMAL_OCT16"){
		return PointAttribute::NORMAL_OCT16;
	}else if(name == "NORMAL"){
		return PointAttribute::NORMAL;
	}

	throw PotreeException("Invalid PointAttribute name: '" + name + "'");
}

bool operator==(const PointAttribute& lhs, const PointAttribute& rhs){ 
	return lhs.ordinal == rhs.ordinal;
}

}