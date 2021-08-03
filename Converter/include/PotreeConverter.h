
#pragma once

#include <execution>

#include "Vector3.h"
#include "LasLoader/LasLoader.h"

struct Dbg {

	bool isDebug = false;

	static Dbg* instance() {

		static Dbg* inst = new Dbg();

		return inst;
	}

};


struct ScaleOffset {
	Vector3 scale;
	Vector3 offset;
};


inline ScaleOffset computeScaleOffset(Vector3 min, Vector3 max, Vector3 targetScale) {

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

	//auto round = [](double number, double step) {
	//	double residue = fmod(number, step);
	//	double n = (step - abs(residue)) * (residue < 0 ? -1.0 : 1.0);

	//	return number + n;
	//};

	//Vector3 roundedOffset = {
	//	round(offset.x, scale.x),
	//	round(offset.y, scale.y),
	//	round(offset.z, scale.z),
	//};
	
	//cout << "offset: " << offset.toString() << endl;
	//cout << "roundedOffset: " << roundedOffset.toString() << endl;

	ScaleOffset scaleOffset;
	scaleOffset.scale = scale;
	scaleOffset.offset = offset;
	//scaleOffset.offset = roundedOffset;
	//scaleOffset.offset = { 0.0, 0.0, 0.0 };

	return scaleOffset;
}



inline vector<Attribute> parseExtraAttributes(LasHeader& header) {

	vector<uint8_t> extraData;

	for (auto& vlr : header.vlrs) {
		if (vlr.recordID == 4) {
			extraData = vlr.data;
			break;
		}
	}

	constexpr int recordSize = 192;
	int numExtraAttributes = extraData.size() / recordSize;
	vector<Attribute> attributes;

	for (int i = 0; i < numExtraAttributes; i++) {

		int offset = i * recordSize;
		uint8_t type = read<uint8_t>(extraData, offset + 2);
		uint8_t options = read<uint8_t>(extraData, offset + 3);
		char chrName[32];
		memcpy(chrName, extraData.data() + offset + 4, 32);
		string name(chrName);

		auto info = lasTypeInfo(type);
		string typeName = getAttributeTypename(info.type);
		int elementSize = getAttributeTypeSize(info.type);

		int size = info.numElements * elementSize;
		Attribute xyz(name, size, info.numElements, elementSize, info.type);

		attributes.push_back(xyz);
	}

	return attributes;
}


inline vector<Attribute> computeOutputAttributes(LasHeader& header) {
	auto format = header.pointDataFormat;

	Attribute xyz("position", 12, 3, 4, AttributeType::INT32);
	Attribute intensity("intensity", 2, 1, 2, AttributeType::UINT16);
	Attribute returns("returns", 1, 1, 1, AttributeType::UINT8);
	Attribute returnNumber("return number", 1, 1, 1, AttributeType::UINT8);
	Attribute numberOfReturns("number of returns", 1, 1, 1, AttributeType::UINT8);
	Attribute classification("classification", 1, 1, 1, AttributeType::UINT8);
	Attribute scanAngleRank("scan angle rank", 1, 1, 1, AttributeType::UINT8);
	Attribute userData("user data", 1, 1, 1, AttributeType::UINT8);
	Attribute pointSourceId("point source id", 2, 1, 2, AttributeType::UINT16);
	Attribute gpsTime("gps-time", 8, 1, 8, AttributeType::DOUBLE);
	Attribute rgb("rgb", 6, 3, 2, AttributeType::UINT16);
	Attribute wavePacketDescriptorIndex("wave packet descriptor index", 1, 1, 1, AttributeType::UINT8);
	Attribute byteOffsetToWaveformData("byte offset to waveform data", 8, 1, 8, AttributeType::UINT64);
	Attribute waveformPacketSize("waveform packet size", 4, 1, 4, AttributeType::UINT32);
	Attribute returnPointWaveformLocation("return point waveform location", 4, 1, 4, AttributeType::FLOAT);
	Attribute XYZt("XYZ(t)", 12, 3, 4, AttributeType::FLOAT);
	Attribute classificationFlags("classification flags", 1, 1, 1, AttributeType::UINT8);
	Attribute scanAngle("scan angle", 2, 1, 2, AttributeType::INT16);

	vector<Attribute> list;

	if (format == 0) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId };
	} else if (format == 1) {
		list = { xyz, intensity, returnNumber, /*numberOfReturns,*/ classification, scanAngleRank, userData, pointSourceId, gpsTime };
	} else if (format == 2) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, rgb };
	} else if (format == 3) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb };
	} else if (format == 4) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime,
			wavePacketDescriptorIndex, byteOffsetToWaveformData, waveformPacketSize, returnPointWaveformLocation,
			XYZt
		};
	} else if (format == 5) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb,
			wavePacketDescriptorIndex, byteOffsetToWaveformData, waveformPacketSize, returnPointWaveformLocation,
			XYZt
		};
	} else if (format == 6) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classificationFlags, classification, userData, scanAngle, pointSourceId, gpsTime };
	} else if (format == 7) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classificationFlags, classification, userData, scanAngle, pointSourceId, gpsTime, rgb };
	} else {
		cout << "ERROR: currently unsupported LAS format: " << int(format) << endl;

		exit(123);
	}

	vector<Attribute> extraAttributes = parseExtraAttributes(header);

	list.insert(list.end(), extraAttributes.begin(), extraAttributes.end());

	return list;
}

inline Attributes computeOutputAttributes(vector<Source>& sources, vector<string> requestedAttributes) {
	// TODO: a bit wasteful to iterate over source files and load headers twice

	Vector3 scaleMin = { Infinity, Infinity, Infinity };
	//Vector3 offset = { Infinity, Infinity, Infinity};
	Vector3 min = { Infinity, Infinity, Infinity };
	Vector3 max = { -Infinity, -Infinity, -Infinity };
	Vector3 scale, offset;

	vector<Attribute> fullAttributeList;
	unordered_map<string, int> acceptedAttributeNames;

	// compute scale and offset from all sources
	{
		mutex mtx;
		auto parallel = std::execution::par;
		for_each(parallel, sources.begin(), sources.end(), [&mtx, &sources, &scaleMin, &min, &max, requestedAttributes, &fullAttributeList, &acceptedAttributeNames](Source source) {

			auto header = loadLasHeader(source.path);

			vector<Attribute> attributes = computeOutputAttributes(header);

			mtx.lock();

			for (auto& attribute : attributes) {
				bool alreadyAdded = acceptedAttributeNames.find(attribute.name) != acceptedAttributeNames.end();

				if (!alreadyAdded) {
					fullAttributeList.push_back(attribute);
					acceptedAttributeNames[attribute.name] = 1;
				}
			}

			scaleMin.x = std::min(scaleMin.x, header.scale.x);
			scaleMin.y = std::min(scaleMin.y, header.scale.y);
			scaleMin.z = std::min(scaleMin.z, header.scale.z);

			min.x = std::min(min.x, header.min.x);
			min.y = std::min(min.y, header.min.y);
			min.z = std::min(min.z, header.min.z);

			max.x = std::max(max.x, header.max.x);
			max.y = std::max(max.y, header.max.y);
			max.z = std::max(max.z, header.max.z);

			mtx.unlock();
			});

		auto scaleOffset = computeScaleOffset(min, max, scaleMin);
		scale = scaleOffset.scale;
		offset = scaleOffset.offset;

		if (scaleMin.x != scale.x || scaleMin.y != scale.y || scaleMin.z != scale.z) {
			GENERATE_WARN_MESSAGE << "scale/offset/bounding box were adjusted. "
				<< "new scale: " << scale.toString() << ", "
				<< "new offset: " << offset.toString() << endl;
		}
	}

	// filter down to optionally specified attributes
	vector<Attribute> filteredAttributeList = fullAttributeList;
	if (requestedAttributes.size() > 0) {
		auto should = requestedAttributes;
		auto is = fullAttributeList;

		// always add position
		should.insert(should.begin(), { "position" });

		vector<Attribute> filtered;

		for (string name : should) {
			auto it = find_if(is.begin(), is.end(), [name](auto& value) {
				return value.name == name;
				});

			if (it != is.end()) {
				filtered.push_back(*it);
			}
		}

		filteredAttributeList = filtered;
	}

	auto attributes = Attributes(filteredAttributeList);
	attributes.posScale = scale;
	attributes.posOffset = offset;



	// { // print infos

	// 	cout << endl << "output attributes: " << endl;

	// 	int c0 = 30;
	// 	int c1 = 10;
	// 	int c2 = 8;
	// 	int ct = c0 + c1 + c2;

	// 	cout << rightPad("name", c0) << leftPad("offset", c1) << leftPad("size", c2) << endl;
	// 	cout << string(ct, '=') << endl;

	// 	int offset = 0;
	// 	for (auto attribute : attributes.list) {
	// 		cout << rightPad(attribute.name, c0)
	// 			<< leftPad(formatNumber(offset), c1)
	// 			<< leftPad(formatNumber(attribute.size), c2)
	// 			<< endl;

	// 		offset += attribute.size;
	// 	}
	// 	cout << string(ct, '=') << endl;

	// 	//cout << "bytes per point: " << attributes.bytes << endl;
	// 	cout << leftPad(formatNumber(attributes.bytes), ct) << endl;
	// 	cout << string(ct, '=') << endl;

	// 	//exit(1234);
	// }

	return attributes;
}

inline string toString(Attributes& attributes){

	stringstream ss;

	ss << endl << "output attributes: " << endl;

	int c0 = 30;
	int c1 = 10;
	int c2 = 8;
	int ct = c0 + c1 + c2;

	ss << rightPad("name", c0) << leftPad("offset", c1) << leftPad("size", c2) << endl;
	ss << string(ct, '=') << endl;

	int offset = 0;
	for (auto attribute : attributes.list) {
		ss << rightPad(attribute.name, c0)
			<< leftPad(formatNumber(offset), c1)
			<< leftPad(formatNumber(attribute.size), c2)
			<< endl;

		offset += attribute.size;
	}
	ss << string(ct, '=') << endl;

	//cout << "bytes per point: " << attributes.bytes << endl;
	ss << leftPad(formatNumber(attributes.bytes), ct) << endl;
	ss << string(ct, '=') << endl;

	return ss.str();
}



