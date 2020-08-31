
#include "LaszipReader.h"

#include "laszip/laszip_api.h"

struct LasTypeInfo1 {
	AttributeType type = AttributeType::UNDEFINED;
	int numElements = 0;
};

LasTypeInfo1 lasTypeInfo(int typeID) {

	unordered_map<int, AttributeType> mapping = {
		{0, AttributeType::UNDEFINED},
		{1, AttributeType::UINT8},
		{2, AttributeType::INT8},
		{3, AttributeType::UINT16},
		{4, AttributeType::INT16},
		{5, AttributeType::UINT32},
		{6, AttributeType::INT32},
		{7, AttributeType::UINT64},
		{8, AttributeType::INT64},
		{9, AttributeType::FLOAT},
		{10, AttributeType::DOUBLE},

		{11, AttributeType::UINT8},
		{12, AttributeType::INT8},
		{13, AttributeType::UINT16},
		{14, AttributeType::INT16},
		{15, AttributeType::UINT32},
		{16, AttributeType::INT32},
		{17, AttributeType::UINT64},
		{18, AttributeType::INT64},
		{19, AttributeType::FLOAT},
		{20, AttributeType::DOUBLE},

		{21, AttributeType::UINT8},
		{22, AttributeType::INT8},
		{23, AttributeType::UINT16},
		{24, AttributeType::INT16},
		{25, AttributeType::UINT32},
		{26, AttributeType::INT32},
		{27, AttributeType::UINT64},
		{28, AttributeType::INT64},
		{29, AttributeType::FLOAT},
		{30, AttributeType::DOUBLE},
	};

	if (mapping.find(typeID) != mapping.end()) {

		AttributeType type = mapping[typeID];

		int numElements = 0;
		if (typeID <= 10) {
			numElements = 1;
		} else if (typeID <= 20) {
			numElements = 2;
		} else if (typeID <= 30) {
			numElements = 3;
		}

		LasTypeInfo1 info;
		info.type = type;
		info.numElements = numElements;

		return info;
	} else {
		cout << "ERROR: unkown extra attribute type: " << typeID << endl;
		exit(123);
	}


}

inline vector<Attribute> parseExtraAttributes(laszip_header* header) {

	vector<uint8_t> extraData;

	int numVlrs = header->number_of_variable_length_records;
	for (int i = 0; i < numVlrs; i++) {
		auto laszip_vlr = header->vlrs[i];

		if (laszip_vlr.record_id == 4) {
			auto extraDataSize = laszip_vlr.record_length_after_header;
			extraData.resize(extraDataSize);

			memcpy(extraData.data(), laszip_vlr.data, extraDataSize);
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


inline vector<Attribute> computeAttributes(laszip_header* header) {
	auto format = header->point_data_format;

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
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime };
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

template<class T>
double asDouble(uint8_t* data) {
	T value = reinterpret_cast<T*>(data)[0];

	return double(value);
}

vector<function<void(int64_t)>> createAttributeHandlers(laszip_header* header, uint8_t* data, laszip_point* point, Attributes& inputAttributes, Attributes& outputAttributes) {

	vector<function<void(int64_t)>> handlers;
	int attributeOffset = 0;

	// reset min/max, we're writing the values to a per-thread copy anyway
	for (auto& attribute : outputAttributes.list) {
		attribute.min = { Infinity, Infinity, Infinity };
		attribute.max = { -Infinity, -Infinity, -Infinity };
	}

	{ // STANDARD LAS ATTRIBUTES

		int offsetPosition = outputAttributes.getOffset("position");
		Attribute* attributePosition = outputAttributes.get("position");
		auto position = [data, point, header, offsetPosition, attributePosition](int64_t offset) {
			if (offsetPosition >= 0) {

				//uint16_t rgb[] = { 0, 0, 0 };
				//memcpy(rgb, &point->rgb, 6);
				//memcpy(data + offset + offsetPosition, rgb, 6);
				memcpy(data + offset + offsetPosition + 0, &point->X, 4);
				memcpy(data + offset + offsetPosition + 4, &point->Y, 4);
				memcpy(data + offset + offsetPosition + 8, &point->Z, 4);

				//attributePosition->min.x = std::min(attributePosition->min.x, double(rgb[0]));
				//attributePosition->min.y = std::min(attributePosition->min.y, double(rgb[1]));
				//attributePosition->min.z = std::min(attributePosition->min.z, double(rgb[2]));

				//attributePosition->max.x = std::max(attributePosition->max.x, double(rgb[0]));
				//attributePosition->max.y = std::max(attributePosition->max.y, double(rgb[1]));
				//attributePosition->max.z = std::max(attributePosition->max.z, double(rgb[2]));
			}
		};

		int offsetRGB = outputAttributes.getOffset("rgb");
		Attribute* attributeRGB = outputAttributes.get("rgb");
		auto rgb = [data, point, header, offsetRGB, attributeRGB](int64_t offset) {
			if (offsetRGB >= 0) {

				uint16_t rgb[] = { 0, 0, 0 };
				memcpy(rgb, &point->rgb, 6);
				memcpy(data + offset + offsetRGB, rgb, 6);

				attributeRGB->min.x = std::min(attributeRGB->min.x, double(rgb[0]));
				attributeRGB->min.y = std::min(attributeRGB->min.y, double(rgb[1]));
				attributeRGB->min.z = std::min(attributeRGB->min.z, double(rgb[2]));

				attributeRGB->max.x = std::max(attributeRGB->max.x, double(rgb[0]));
				attributeRGB->max.y = std::max(attributeRGB->max.y, double(rgb[1]));
				attributeRGB->max.z = std::max(attributeRGB->max.z, double(rgb[2]));
			}
		};

		int offsetIntensity = outputAttributes.getOffset("intensity");
		Attribute* attributeIntensity = outputAttributes.get("intensity");
		auto intensity = [data, point, header, offsetIntensity, attributeIntensity](int64_t offset) {
			memcpy(data + offset + offsetIntensity, &point->intensity, 2);

			attributeIntensity->min.x = std::min(attributeIntensity->min.x, double(point->intensity));
			attributeIntensity->max.x = std::max(attributeIntensity->max.x, double(point->intensity));
		};

		int offsetReturnNumber = outputAttributes.getOffset("return number");
		Attribute* attributeReturnNumber = outputAttributes.get("return number");
		auto returnNumber = [data, point, header, offsetReturnNumber, attributeReturnNumber](int64_t offset) {
			uint8_t value = point->return_number;

			memcpy(data + offset + offsetReturnNumber, &value, 1);

			attributeReturnNumber->min.x = std::min(attributeReturnNumber->min.x, double(value));
			attributeReturnNumber->max.x = std::max(attributeReturnNumber->max.x, double(value));
		};

		int offsetNumberOfReturns = outputAttributes.getOffset("number of returns");
		Attribute* attributeNumberOfReturns = outputAttributes.get("number of returns");
		auto numberOfReturns = [data, point, header, offsetNumberOfReturns, attributeNumberOfReturns](int64_t offset) {
			uint8_t value = point->number_of_returns;

			memcpy(data + offset + offsetNumberOfReturns, &value, 1);

			attributeNumberOfReturns->min.x = std::min(attributeNumberOfReturns->min.x, double(value));
			attributeNumberOfReturns->max.x = std::max(attributeNumberOfReturns->max.x, double(value));
		};

		int offsetScanAngleRank = outputAttributes.getOffset("scan angle rank");
		Attribute* attributeScanAngleRank = outputAttributes.get("scan angle rank");
		auto scanAngleRank = [data, point, header, offsetScanAngleRank, attributeScanAngleRank](int64_t offset) {
			memcpy(data + offset + offsetScanAngleRank, &point->scan_angle_rank, 1);

			attributeScanAngleRank->min.x = std::min(attributeScanAngleRank->min.x, double(point->scan_angle_rank));
			attributeScanAngleRank->max.x = std::max(attributeScanAngleRank->max.x, double(point->scan_angle_rank));
		};

		int offsetScanAngle = outputAttributes.getOffset("scan angle");
		Attribute* attributeScanAngle = outputAttributes.get("scan angle");
		auto scanAngle = [data, point, header, offsetScanAngle, attributeScanAngle](int64_t offset) {
			memcpy(data + offset + offsetScanAngle, &point->extended_scan_angle, 2);

			attributeScanAngle->min.x = std::min(attributeScanAngle->min.x, double(point->extended_scan_angle));
			attributeScanAngle->max.x = std::max(attributeScanAngle->max.x, double(point->extended_scan_angle));
		};

		int offsetUserData = outputAttributes.getOffset("user data");
		Attribute* attributeUserData = outputAttributes.get("user data");
		auto userData = [data, point, header, offsetUserData, attributeUserData](int64_t offset) {
			memcpy(data + offset + offsetUserData, &point->user_data, 1);

			attributeUserData->min.x = std::min(attributeUserData->min.x, double(point->user_data));
			attributeUserData->max.x = std::max(attributeUserData->max.x, double(point->user_data));
		};

		int offsetClassification = outputAttributes.getOffset("classification");
		Attribute* attributeClassification = outputAttributes.get("classification");
		auto classification = [data, point, header, offsetClassification, attributeClassification](int64_t offset) {
			data[offset + offsetClassification] = point->classification;

			attributeClassification->min.x = std::min(attributeClassification->min.x, double(point->classification));
			attributeClassification->max.x = std::max(attributeClassification->max.x, double(point->classification));
		};

		int offsetSourceId = outputAttributes.getOffset("point source id");
		Attribute* attributePointSourceId = outputAttributes.get("point source id");
		auto pointSourceId = [data, point, header, offsetSourceId, attributePointSourceId](int64_t offset) {
			memcpy(data + offset + offsetSourceId, &point->point_source_ID, 2);

			attributePointSourceId->min.x = std::min(attributePointSourceId->min.x, double(point->point_source_ID));
			attributePointSourceId->max.x = std::max(attributePointSourceId->max.x, double(point->point_source_ID));
		};

		int offsetGpsTime = outputAttributes.getOffset("gps-time");
		Attribute* attributeGpsTime = outputAttributes.get("gps-time");
		auto gpsTime = [data, point, header, offsetGpsTime, attributeGpsTime](int64_t offset) {
			memcpy(data + offset + offsetGpsTime, &point->gps_time, 8);

			attributeGpsTime->min.x = std::min(attributeGpsTime->min.x, point->gps_time);
			attributeGpsTime->max.x = std::max(attributeGpsTime->max.x, point->gps_time);
		};

		int offsetClassificationFlags = outputAttributes.getOffset("classification flags");
		Attribute* attributeClassificationFlags = outputAttributes.get("classification flags");
		auto classificationFlags = [data, point, header, offsetClassificationFlags, attributeClassificationFlags](int64_t offset) {
			uint8_t value = point->extended_classification_flags;

			memcpy(data + offset + offsetClassificationFlags, &value, 1);

			attributeClassificationFlags->min.x = std::min(attributeClassificationFlags->min.x, double(point->extended_classification_flags));
			attributeClassificationFlags->max.x = std::max(attributeClassificationFlags->max.x, double(point->extended_classification_flags));
		};

		unordered_map<string, function<void(int64_t)>> mapping = {
			{"position", position},
			{"rgb", rgb},
			{"intensity", intensity},
			{"return number", returnNumber},
			{"number of returns", numberOfReturns},
			{"classification", classification},
			{"scan angle rank", scanAngleRank},
			{"scan angle", scanAngle},
			{"user data", userData},
			{"point source id", pointSourceId},
			{"gps-time", gpsTime},
			{"classification flags", classificationFlags},
		};

		for (auto& attribute : inputAttributes.list) {

			attributeOffset += attribute.size;

			if (attribute.name == "position") {
				continue;
			}

			bool standardMappingExists = mapping.find(attribute.name) != mapping.end();
			bool isIncludedInOutput = outputAttributes.get(attribute.name) != nullptr;
			if (standardMappingExists && isIncludedInOutput) {
				handlers.push_back(mapping[attribute.name]);
			}
		}
	}

	{ // EXTRA ATTRIBUTES

		// mapping from las format to index of first extra attribute
		// +1 for all formats with returns, which is split into return number and number of returns
		unordered_map<int, int> formatToExtraIndex = {
			{0, 8},
			{1, 9},
			{2, 9},
			{3, 10},
			{4, 14},
			{5, 15},
			{6, 10},
			{7, 11},
		};

		bool noMapping = formatToExtraIndex.find(header->point_data_format) == formatToExtraIndex.end();
		if (noMapping) {
			string msg = "ERROR: las format not supported: " + formatNumber(header->point_data_format) + "\n";
			cout << msg;

			exit(123);
		}

		// handle extra bytes individually to compute per-attribute information
		int firstExtraIndex = formatToExtraIndex[header->point_data_format];
		int sourceOffset = 0;

		int attributeOffset = 0;
		for (int i = 0; i < firstExtraIndex; i++) {
			attributeOffset += inputAttributes.list[i].size;
		}

		for (int i = firstExtraIndex; i < inputAttributes.list.size(); i++) {
			Attribute& inputAttribute = inputAttributes.list[i];
			Attribute* attribute = outputAttributes.get(inputAttribute.name);
			int targetOffset = outputAttributes.getOffset(inputAttribute.name);

			int attributeSize = inputAttribute.size;

			if (attribute != nullptr) {
				auto handleAttribute = [data, point, header, attributeSize, attributeOffset, sourceOffset, attribute](int64_t offset) {
					memcpy(data + offset + attributeOffset, point->extra_bytes + sourceOffset, attributeSize);

					std::function<double(uint8_t*)> f;

					// TODO: shouldn't use DOUBLE as a unifying type
					// it won't work with uint64_t and int64_t
					if (attribute->type == AttributeType::INT8) {
						f = asDouble<int8_t>;
					} else if (attribute->type == AttributeType::INT16) {
						f = asDouble<int16_t>;
					} else if (attribute->type == AttributeType::INT32) {
						f = asDouble<int32_t>;
					} else if (attribute->type == AttributeType::INT64) {
						f = asDouble<int64_t>;
					} else if (attribute->type == AttributeType::UINT8) {
						f = asDouble<uint8_t>;
					} else if (attribute->type == AttributeType::UINT16) {
						f = asDouble<uint16_t>;
					} else if (attribute->type == AttributeType::UINT32) {
						f = asDouble<uint32_t>;
					} else if (attribute->type == AttributeType::UINT64) {
						f = asDouble<uint64_t>;
					} else if (attribute->type == AttributeType::FLOAT) {
						f = asDouble<float>;
					} else if (attribute->type == AttributeType::DOUBLE) {
						f = asDouble<double>;
					}

					if (attribute->numElements == 1) {
						double x = f(point->extra_bytes + sourceOffset);

						attribute->min.x = std::min(attribute->min.x, x);
						attribute->max.x = std::max(attribute->max.x, x);
					} else if (attribute->numElements == 2) {
						double x = f(point->extra_bytes + sourceOffset + 0 * attribute->elementSize);
						double y = f(point->extra_bytes + sourceOffset + 1 * attribute->elementSize);

						attribute->min.x = std::min(attribute->min.x, x);
						attribute->min.y = std::min(attribute->min.y, y);
						attribute->max.x = std::max(attribute->max.x, x);
						attribute->max.y = std::max(attribute->max.y, y);

					} else if (attribute->numElements == 3) {
						double x = f(point->extra_bytes + sourceOffset + 0 * attribute->elementSize);
						double y = f(point->extra_bytes + sourceOffset + 1 * attribute->elementSize);
						double z = f(point->extra_bytes + sourceOffset + 2 * attribute->elementSize);

						attribute->min.x = std::min(attribute->min.x, x);
						attribute->min.y = std::min(attribute->min.y, y);
						attribute->min.z = std::min(attribute->min.z, z);
						attribute->max.x = std::max(attribute->max.x, x);
						attribute->max.y = std::max(attribute->max.y, y);
						attribute->max.z = std::max(attribute->max.z, z);
					}


				};

				handlers.push_back(handleAttribute);
			}

			sourceOffset += attribute->size;
			attributeOffset += attribute->size;
		}

	}


	return handlers;

}

Points readLasLaz(string path, int firstPoint, int numPoints) {

	laszip_POINTER laszip_reader = nullptr;
	laszip_header* header = nullptr;
	laszip_point* point = nullptr;

	{
		laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
		laszip_BOOL request_reader = 1;

		laszip_create(&laszip_reader);
		laszip_request_compatibility_mode(laszip_reader, request_reader);
		laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);
		laszip_get_header_pointer(laszip_reader, &header);
		laszip_get_point_pointer(laszip_reader, &point);
		laszip_seek_point(laszip_reader, firstPoint);
	}

	//int64_t numPoints = std::max(
	//	uint64_t(header->number_of_point_records), 
	//	header->extended_number_of_point_records);
	int64_t bpp = header->point_data_record_length;
	int64_t dataSize = numPoints * bpp;

	Points points;
	points.data = make_shared<Buffer>(dataSize);
	points.attributes = computeAttributes(header);
	points.numPoints = numPoints;

	auto attributeHandlers = createAttributeHandlers(header, points.data->data_u8, point, points.attributes, points.attributes);
	
	for (int i = 0; i < numPoints; i++) {
		int64_t pointOffset = i * bpp;

		laszip_read_point(laszip_reader);

		for (auto& handler : attributeHandlers) {
			handler(pointOffset);
		}

	}

	return points;
}