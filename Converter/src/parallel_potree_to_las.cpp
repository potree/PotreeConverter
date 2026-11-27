#include "parallel_potree_to_las.h"
#include "json/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <thread>
#include "laszip/laszip_api.h"
#include "brotli/decode.h"

namespace fs = std::filesystem;

// Attributes that map to standard las point fields. Anything else is written as las extra bytes.
static const std::vector<std::string> standardAttributes20 = {
	"position", "intensity", "returns", "return number", "number of returns", "classification",
	"scan angle rank", "user data", "point source id", "gps-time", "classification flags",
	"scan angle", "rgb", "wave packet descriptor index", "byte offset to waveform data",
	"waveform packet size", "return point waveform location", "XYZ(t)"
};

static const std::vector<std::string> standardAttributes17 = {
	"POSITION_CARTESIAN", "RGBA", "COLOR_PACKED", "intensity", "INTENSITY", "classification",
	"CLASSIFICATION", "return number", "number of returns", "source id", "gps-time",
	"NORMAL_SPHEREMAPPED", "NORMAL_OCT16", "NORMAL"
};

static bool contains(const std::vector<std::string>& list, const std::string& value)
{
	return std::find(list.begin(), list.end(), value) != list.end();
}

static Vector3 readVector3(json& js, Vector3 fallback)
{
	if (!js.is_array())
	{
		return fallback;
	}

	Vector3 value = fallback;
	if (js.size() > 0) value.x = js[0].get<double>();
	if (js.size() > 1) value.y = js[1].get<double>();
	if (js.size() > 2) value.z = js[2].get<double>();

	return value;
}

inline Attributes getAttributes(json& jsMetadata, parallel_handler* ph) {
	vector<Attribute> attributeList;

	if (ph->version == "2.0")
	{
		auto jsAttributes = jsMetadata["attributes"];

		for (auto jsAttribute : jsAttributes)
		{
			string name = jsAttribute["name"];
			string description = jsAttribute.value("description", "");
			int size = jsAttribute["size"];
			int numElements = jsAttribute["numElements"];
			int elementSize = jsAttribute["elementSize"];
			AttributeType type = typenameToType(jsAttribute["type"]);

			Attribute attribute(name, size, numElements, elementSize, type);
			attribute.description = description;
			attribute.scale = readVector3(jsAttribute["scale"], attribute.scale);
			attribute.offset = readVector3(jsAttribute["offset"], attribute.offset);

			if (!contains(standardAttributes20, name))
			{
				attribute.byte_index = ph->extra_bytes;
				ph->extra_bytes += attribute.size;
				std::cout << "Detected Extra attribute - Name: " << name << " Description: " << description << " Size: " << size << std::endl;
			}

			attributeList.push_back(attribute);
		}

		Attributes attributes(attributeList);
		attributes.posScale = readVector3(jsMetadata["scale"], attributes.posScale);
		attributes.posOffset = readVector3(jsMetadata["offset"], attributes.posOffset);

		return attributes;
	}
	else
	{
		auto jsAttributes = jsMetadata["pointAttributes"];
		if (!jsAttributes.is_array())
		{
			std::cout << "pointAttributes attribute in cloud.js is not an array! Exiting." << std::endl;
			exit(1);
		}
		if (jsAttributes.empty())
		{
			std::cout << "pointAttributes array attribute in cloud.js is empty! Exiting." << std::endl;
			exit(1);
		}

		if (jsAttributes[0].is_object()) // Handle 1.7 format. version: "1.8".
		{
			for (auto jsAttribute : jsAttributes) {
				string name = jsAttribute["name"];
				string description = jsAttribute.value("description", "");
				int size = jsAttribute["size"];
				int numElements = jsAttribute["elements"];
				int elementSize = jsAttribute["elementSize"];
				AttributeType type = typenameToType(jsAttribute["type"]);

				Attribute attribute(name, size, numElements, elementSize, type);
				attribute.description = description;
				if (!contains(standardAttributes17, name))
				{
					attribute.byte_index = ph->extra_bytes;
					ph->extra_bytes += attribute.size;
					std::cout << "Detected Extra attribute - Name: " << name << " Description: " << description << " Size: " << size << std::endl;
				}

				attributeList.push_back(attribute);
			}
		}
		else if (jsAttributes[0].is_string()) // Handle 1.5 format. version: "1.7"
		{
			for (auto jsAttribute : jsAttributes) {
				string name = jsAttribute;

				int numElements;
				int elementSize;
				AttributeType type;
				if (name == "POSITION_CARTESIAN")
				{
					numElements = 3;
					elementSize = 4;
					type = AttributeType::INT32;
				}
				else if (name == "COLOR_PACKED")
				{
					numElements = 4;
					elementSize = 1;
					type = AttributeType::UINT8;
				}
				else if (name == "INTENSITY")
				{
					numElements = 1;
					elementSize = 2;
					type = AttributeType::UINT16;
				}
				else if (name == "CLASSIFICATION")
				{
					numElements = 1;
					elementSize = 1;
					type = AttributeType::UINT8;
				}
				else if (name == "NORMAL_SPHEREMAPPED" || name == "NORMAL_OCT16")
				{
					numElements = 2;
					elementSize = 1;
					type = AttributeType::UINT8;
				}
				else if (name == "NORMAL")
				{
					numElements = 3;
					elementSize = 4;
					type = AttributeType::FLOAT;
				}
				else
				{
					std::cout << "Unknown point attribute in cloud.js: " << name << ". Exiting." << std::endl;
					exit(1);
				}

				// TODO: Add compatibility for extra attributes.
				attributeList.push_back(Attribute(name, numElements * elementSize, numElements, elementSize, type));
			}
		}

		double scale = jsMetadata["scale"];

		Attributes attributes(attributeList);
		attributes.posScale = { scale, scale, scale };
		// 1.7 positions are relative to the bounding box of their node.
		attributes.posOffset = ph->min;

		return attributes;
	}
}

void parallel_handler::parseMetadata(std::string source_path)
{
	fs::path path(source_path);
	fs::path cloud_js_path = path / "cloud.js";
	fs::path sources_json_path = path / "sources.json";
	fs::path metadata_json_path = path / "metadata.json";

	if (fs::exists(metadata_json_path))
	{
		std::cout << "Potree Converter 2.0 files detected!" << std::endl;

		version = "2.0";

		js = json::parse(readTextFile(metadata_json_path.string()));
		las_name = js["name"].get<std::string>();
		encoding = js["encoding"].get<std::string>();

		json jsBoundingBox = js["boundingBox"];
		min = readVector3(jsBoundingBox["min"], min);
		max = readVector3(jsBoundingBox["max"], max);
	}
	else if (fs::exists(cloud_js_path) && fs::exists(sources_json_path))
	{
		std::cout << "Potree Converter 1.7 files detected!" << std::endl;

		version = "1.7";

		js = json::parse(readTextFile(cloud_js_path.string()));
		json jsSourcesJson = json::parse(readTextFile(sources_json_path.string()));

		las_name = fs::path(jsSourcesJson["sources"][0]["name"].get<std::string>()).stem().string();

		// The octree hierarchy is built on the cubic bounding box.
		json jsBoundingBox = js["boundingBox"];
		min = { jsBoundingBox["lx"].get<double>(), jsBoundingBox["ly"].get<double>(), jsBoundingBox["lz"].get<double>() };
		max = { jsBoundingBox["ux"].get<double>(), jsBoundingBox["uy"].get<double>(), jsBoundingBox["uz"].get<double>() };

		expected_number_of_points = js["points"];
		octreeDir = js["octreeDir"].get<std::string>();
	}
	else
	{
		std::cout << "No metadata.json (2.0) or cloud.js + sources.json (1.7) found in " << source_path << ". Exiting." << std::endl;
		exit(1);
	}
}

parallel_handler::parallel_handler(std::string source_path, std::string target_path)
{
	this->points_queue = new tbb::concurrent_bounded_queue<PointsArray*>();
	this->points_queue->set_capacity(64);
	this->source_path = source_path;
	this->target_path = target_path;
	std::cout << "Source Path: " << source_path << std::endl;
	std::cout << "Target Path: " << target_path << std::endl;
	std::cout << "Read metadata" << std::endl;
	this->parseMetadata(source_path);
	std::cout << "Read Attributes" << std::endl;
	attributes = getAttributes(this->js, this);
}

parallel_handler::~parallel_handler()
{
	delete this->points_queue;
}

// Byte offsets of the standard attributes inside a point record, -1 if the attribute doesn't exist.
struct AttributeOffsets
{
	int position = -1;
	int rgb = -1;
	int rgba = -1;
	int intensity = -1;
	int return_number = -1;
	int number_of_returns = -1;
	int classification = -1;
	int classification_flags = -1;
	int scan_angle_rank = -1;
	int scan_angle = -1;
	int user_data = -1;
	int point_source_id = -1;
	int gps_time = -1;

	struct Extra
	{
		int source_offset;
		int target_offset;
		int size;
	};
	std::vector<Extra> extras;

	AttributeOffsets(Attributes& attributes, std::string version)
	{
		if (version == "2.0")
		{
			position = attributes.getOffset("position");
			rgb = attributes.getOffset("rgb");
			intensity = attributes.getOffset("intensity");
			return_number = attributes.getOffset("return number");
			number_of_returns = attributes.getOffset("number of returns");
			classification = attributes.getOffset("classification");
			classification_flags = attributes.getOffset("classification flags");
			scan_angle_rank = attributes.getOffset("scan angle rank");
			scan_angle = attributes.getOffset("scan angle");
			user_data = attributes.getOffset("user data");
			point_source_id = attributes.getOffset("point source id");
			gps_time = attributes.getOffset("gps-time");
		}
		else
		{
			position = attributes.getOffset("POSITION_CARTESIAN");
			rgba = std::max(attributes.getOffset("RGBA"), attributes.getOffset("COLOR_PACKED"));
			intensity = std::max(attributes.getOffset("intensity"), attributes.getOffset("INTENSITY"));
			return_number = attributes.getOffset("return number");
			number_of_returns = attributes.getOffset("number of returns");
			classification = std::max(attributes.getOffset("classification"), attributes.getOffset("CLASSIFICATION"));
			point_source_id = attributes.getOffset("source id");
			gps_time = attributes.getOffset("gps-time");
		}

		for (Attribute& attribute : attributes.getExtraAttributes())
		{
			extras.push_back({ attributes.getOffset(attribute.name), attribute.byte_index, attribute.size });
		}
	}
};

template<typename T>
static inline T readValue(const uint8_t* data)
{
	T value;
	memcpy(&value, data, sizeof(T));
	return value;
}

static PointsArray* decodePoints(parallel_handler* ph, AttributeOffsets& offsets, const uint8_t* buffer, uint64_t numPoints, Vector3 positionOffset)
{
	int bpp = ph->attributes.bytes;
	Vector3 scale = ph->attributes.posScale;

	PointsArray* pa = new PointsArray();
	pa->points.resize(numPoints);
	pa->extra_bytes.resize(numPoints * ph->extra_bytes);

	for (uint64_t i = 0; i < numPoints; i++)
	{
		const uint8_t* record = buffer + i * bpp;
		Point& point = pa->points[i];

		if (ph->version == "2.0")
		{
			point.x = double(readValue<int32_t>(record + offsets.position + 0)) * scale.x + positionOffset.x;
			point.y = double(readValue<int32_t>(record + offsets.position + 4)) * scale.y + positionOffset.y;
			point.z = double(readValue<int32_t>(record + offsets.position + 8)) * scale.z + positionOffset.z;
		}
		else
		{
			point.x = double(readValue<uint32_t>(record + offsets.position + 0)) * scale.x + positionOffset.x;
			point.y = double(readValue<uint32_t>(record + offsets.position + 4)) * scale.y + positionOffset.y;
			point.z = double(readValue<uint32_t>(record + offsets.position + 8)) * scale.z + positionOffset.z;
		}

		if (offsets.rgb >= 0)
		{
			point.r = readValue<uint16_t>(record + offsets.rgb + 0);
			point.g = readValue<uint16_t>(record + offsets.rgb + 2);
			point.b = readValue<uint16_t>(record + offsets.rgb + 4);
		}
		if (offsets.rgba >= 0)
		{
			point.r = record[offsets.rgba + 0];
			point.g = record[offsets.rgba + 1];
			point.b = record[offsets.rgba + 2];
		}
		if (offsets.intensity >= 0) point.intensity = readValue<uint16_t>(record + offsets.intensity);
		if (offsets.return_number >= 0) point.return_number = record[offsets.return_number];
		if (offsets.number_of_returns >= 0) point.number_of_returns = record[offsets.number_of_returns];
		if (offsets.classification >= 0) point.classification = record[offsets.classification];
		if (offsets.classification_flags >= 0) point.classification_flags = record[offsets.classification_flags];
		if (offsets.scan_angle_rank >= 0) point.scan_angle_rank = readValue<int8_t>(record + offsets.scan_angle_rank);
		if (offsets.scan_angle >= 0) point.scan_angle = readValue<int16_t>(record + offsets.scan_angle);
		if (offsets.user_data >= 0) point.user_data = record[offsets.user_data];
		if (offsets.point_source_id >= 0) point.point_source_ID = readValue<uint16_t>(record + offsets.point_source_id);
		if (offsets.gps_time >= 0) point.gps_time = readValue<double>(record + offsets.gps_time);

		uint8_t* extra = pa->extra_bytes.data() + i * ph->extra_bytes;
		for (auto& e : offsets.extras)
		{
			memcpy(extra + e.target_offset, record + e.source_offset, e.size);
		}
	}

	return pa;
}

void fillPointsDEFAULT(parallel_handler* ph)
{
	AttributeOffsets offsets(ph->attributes, ph->version);
	std::string octreePath = ph->source_path + "/octree.bin";
	std::vector<uint8_t> buffer;

	for (size_t index = ph->next_node++; index < ph->nodes.size(); index = ph->next_node++)
	{
		auto& node = ph->nodes[index];

		uint64_t expectedSize = uint64_t(node->numPoints) * ph->attributes.bytes;
		if (node->byteSize != expectedSize)
		{
			std::cout << "Warning: node " << node->name << " has " << node->byteSize << " bytes but "
				<< node->numPoints << " points need " << expectedSize << " bytes." << std::endl;
		}

		buffer.resize(node->byteSize);
		readBinaryFile(octreePath, node->byteOffset, node->byteSize, buffer.data());

		uint64_t numPoints = std::min<uint64_t>(node->numPoints, node->byteSize / ph->attributes.bytes);
		ph->points_queue->push(decodePoints(ph, offsets, buffer.data(), numPoints, ph->attributes.posOffset));
	}
}

// Inverse of splitBy3 in converter_utils.h: collects every third bit.
static inline uint32_t compactBy3(uint64_t x)
{
	x &= 0x1249249249249249;
	x = (x | x >> 2) & 0x10c30c30c30c30c3;
	x = (x | x >> 4) & 0x100f00f00f00f00f;
	x = (x | x >> 8) & 0x1f0000ff0000ff;
	x = (x | x >> 16) & 0x1f00000000ffff;
	x = (x | x >> 32) & 0x1fffff;
	return uint32_t(x);
}

// Reverses compress() in Writer.cpp: the brotli stream holds one array per attribute, all in the
// same order. Positions are 2 morton codes (upper and lower 16 bits), rgb is one morton code.
// The arrays are interleaved back into DEFAULT records, so decodePoints() can be reused.
void fillPointsBROTLI(parallel_handler* ph)
{
	AttributeOffsets offsets(ph->attributes, ph->version);
	std::string octreePath = ph->source_path + "/octree.bin";
	std::vector<uint8_t> buffer, decoded, records;
	int bpp = ph->attributes.bytes;

	for (size_t index = ph->next_node++; index < ph->nodes.size(); index = ph->next_node++)
	{
		auto& node = ph->nodes[index];
		uint64_t numPoints = node->numPoints;

		buffer.resize(node->byteSize);
		readBinaryFile(octreePath, node->byteOffset, node->byteSize, buffer.data());

		size_t decodedSize = 0;
		for (Attribute& attribute : ph->attributes.list)
		{
			int size = attribute.name == "position" ? 16 : (attribute.name == "rgb" ? 8 : attribute.size);
			decodedSize += size * numPoints;
		}
		decoded.resize(decodedSize);

		size_t outSize = decodedSize;
		if (BrotliDecoderDecompress(buffer.size(), buffer.data(), &outSize, decoded.data()) != BROTLI_DECODER_RESULT_SUCCESS || outSize != decodedSize)
		{
			std::cout << "ERROR: failed to decompress node " << node->name << ", expected " << decodedSize << " bytes, got " << outSize << ". Exiting." << std::endl;
			exit(1);
		}

		records.resize(numPoints * bpp);
		const uint8_t* source = decoded.data();
		for (Attribute& attribute : ph->attributes.list)
		{
			int offset = ph->attributes.getOffset(attribute.name);

			for (uint64_t i = 0; i < numPoints; i++)
			{
				uint8_t* record = records.data() + i * bpp + offset;

				if (attribute.name == "position")
				{
					uint64_t upper = readValue<uint64_t>(source + 16 * i + 0);
					uint64_t lower = readValue<uint64_t>(source + 16 * i + 8);
					int32_t xyz[3];
					for (int axis = 0; axis < 3; axis++)
					{
						xyz[axis] = int32_t((compactBy3(upper >> axis) << 16) | compactBy3(lower >> axis));
					}
					memcpy(record, xyz, 12);
				}
				else if (attribute.name == "rgb")
				{
					uint64_t mc = readValue<uint64_t>(source + 8 * i);
					uint16_t rgb[3] = { uint16_t(compactBy3(mc)), uint16_t(compactBy3(mc >> 1)), uint16_t(compactBy3(mc >> 2)) };
					memcpy(record, rgb, 6);
				}
				else
				{
					memcpy(record, source + attribute.size * i, attribute.size);
				}
			}

			source += numPoints * (attribute.name == "position" ? 16 : (attribute.name == "rgb" ? 8 : attribute.size));
		}

		ph->points_queue->push(decodePoints(ph, offsets, records.data(), numPoints, ph->attributes.posOffset));
	}
}

void fillPointsDEFAULT17(parallel_handler* ph)
{
	AttributeOffsets offsets(ph->attributes, ph->version);
	std::vector<uint8_t> buffer;

	for (size_t index = ph->next_node++; index < ph->nodes.size(); index = ph->next_node++)
	{
		auto& node = ph->nodes[index];

		// Determine the node bounding box from its name, e.g. "r0372".
		fs::path full_path = node->name;
		std::string node_name = full_path.stem().string();
		BoundingBox box = { ph->min, ph->max };
		for (int i = 1; i < node_name.size(); i++) {
			int childIndex = node_name[i] - '0';
			box = childBoundingBoxOf(box.min, box.max, childIndex);
		}

		buffer.resize(node->byteSize);
		readBinaryFile(full_path.string(), 0, node->byteSize, buffer.data());

		uint64_t numPoints = node->byteSize / ph->attributes.bytes;
		ph->points_queue->push(decodePoints(ph, offsets, buffer.data(), numPoints, box.min));
	}
}

// Las extra bytes data types, see the LAS 1.4 specification, table 24.
static int lasExtraBytesType(AttributeType type)
{
	switch (type)
	{
		case AttributeType::UINT8: return 1;
		case AttributeType::INT8: return 2;
		case AttributeType::UINT16: return 3;
		case AttributeType::INT16: return 4;
		case AttributeType::UINT32: return 5;
		case AttributeType::INT32: return 6;
		case AttributeType::UINT64: return 7;
		case AttributeType::INT64: return 8;
		case AttributeType::FLOAT: return 9;
		case AttributeType::DOUBLE: return 10;
		default: return 0; // undocumented extra bytes
	}
}

// Builds the "LASF_Spec" record 4 VLR. laszip_add_attribute() always flags scale and offset as used,
// which would turn plain extra bytes into scaled ones, so the records are written here directly.
static std::vector<uint8_t> createExtraBytesVLR(std::vector<Attribute>& extraAttributes)
{
	constexpr int recordSize = 192;
	std::vector<uint8_t> vlr(recordSize * extraAttributes.size(), 0);

	for (size_t i = 0; i < extraAttributes.size(); i++)
	{
		Attribute& attribute = extraAttributes[i];
		uint8_t* record = vlr.data() + i * recordSize;

		int type = lasExtraBytesType(attribute.type);
		uint8_t options = 0;
		if (type == 0)
		{
			// for undocumented extra bytes, the options field holds the number of bytes.
			options = attribute.size;
		}
		if (type != 0 && attribute.scale.x != 1.0)
		{
			options |= 0x08;
		}
		if (type != 0 && attribute.offset.x != 0.0)
		{
			options |= 0x10;
		}

		record[2] = type;
		record[3] = options;
		memcpy(record + 4, attribute.name.c_str(), std::min<size_t>(attribute.name.size(), 32));
		memcpy(record + 112, &attribute.scale.x, 8);
		memcpy(record + 136, &attribute.offset.x, 8);
		memcpy(record + 160, attribute.description.c_str(), std::min<size_t>(attribute.description.size(), 32));
	}

	return vlr;
}

static bool checkLaszip(laszip_POINTER laszip, laszip_I32 result, const char* action)
{
	if (result == 0)
	{
		return true;
	}

	laszip_CHAR* error = nullptr;
	laszip_get_error(laszip, &error);
	std::cout << "ERROR: " << action << " failed: " << (error ? error : "unknown error") << std::endl;

	return false;
}

// Pops points until the nullptr end marker, so the producers never block on a full queue.
static void drainPoints(parallel_handler* ph)
{
	PointsArray* points_array;
	while (true)
	{
		ph->points_queue->pop(points_array);
		if (points_array == nullptr)
		{
			return;
		}
		delete points_array;
	}
}

void save(parallel_handler* ph, bool* success) {
	*success = false;

	std::string target = (fs::path(ph->target_path) / (ph->las_name + ".las")).string();
	Attributes& attributes = ph->attributes;
	AttributeOffsets offsets(attributes, ph->version);

	bool hasRgb = offsets.rgb >= 0 || offsets.rgba >= 0;
	bool hasGpsTime = offsets.gps_time >= 0;
	// "scan angle" and "classification flags" only exist for las point formats 6+.
	bool isExtended = offsets.scan_angle >= 0 || offsets.classification_flags >= 0;

	int pointFormat;
	if (isExtended)
	{
		pointFormat = hasRgb ? 7 : 6;
	}
	else
	{
		pointFormat = hasGpsTime ? (hasRgb ? 3 : 1) : (hasRgb ? 2 : 0);
	}
	int baseRecordLength[] = { 20, 28, 26, 34, 0, 0, 30, 36 };

	laszip_POINTER laszip_writer;
	laszip_point* laszip_point;
	laszip_header* header;

	std::vector<Attribute> extraAttributeVector = attributes.getExtraAttributes();

	laszip_create(&laszip_writer);
	laszip_get_header_pointer(laszip_writer, &header);
	header->version_major = 1;
	header->version_minor = (isExtended || ph->extra_bytes > 0) ? 4 : 2;
	header->header_size = header->version_minor == 4 ? 375 : 227;
	header->offset_to_point_data = header->header_size;
	header->point_data_format = pointFormat;
	header->point_data_record_length = baseRecordLength[pointFormat] + ph->extra_bytes;
	// Counts and bounds are filled in by the inventory when the writer is closed.
	header->number_of_point_records = 0;
	header->extended_number_of_point_records = 0;
	header->x_scale_factor = attributes.posScale.x;
	header->y_scale_factor = attributes.posScale.y;
	header->z_scale_factor = attributes.posScale.z;

	header->x_offset = attributes.posOffset.x;
	header->y_offset = attributes.posOffset.y;
	header->z_offset = attributes.posOffset.z;

	header->min_x = ph->min.x;
	header->min_y = ph->min.y;
	header->min_z = ph->min.z;

	header->max_x = ph->max.x;
	header->max_y = ph->max.y;
	header->max_z = ph->max.z;

	bool ok = true;

	if (!extraAttributeVector.empty())
	{
		std::vector<uint8_t> vlr = createExtraBytesVLR(extraAttributeVector);
		ok = checkLaszip(laszip_writer, laszip_add_vlr(laszip_writer, "LASF_Spec", 4, vlr.size(), "Extra Bytes", vlr.data()), "adding extra bytes VLR");
	}

	std::cout << "Target location: " << target << " (LAS 1." << int(header->version_minor) << ", point format " << pointFormat << ")" << std::endl;
	ok = ok && checkLaszip(laszip_writer, laszip_open_writer(laszip_writer, target.c_str(), false), "opening las writer");
	ok = ok && checkLaszip(laszip_writer, laszip_get_point_pointer(laszip_writer, &laszip_point), "getting las point");

	if (!ok)
	{
		drainPoints(ph);
		laszip_destroy(laszip_writer);
		return;
	}

	double coordinates[3];
	auto lastReport = std::chrono::steady_clock::now();

	while (ok) {
		PointsArray* points_array;
		ph->points_queue->pop(points_array);

		if (points_array == nullptr)
		{
			break;
		}

		for (size_t i = 0; i < points_array->points.size() && ok; i++)
		{
			Point& point = points_array->points[i];

			coordinates[0] = point.x;
			coordinates[1] = point.y;
			coordinates[2] = point.z;

			laszip_point->rgb[0] = point.r;
			laszip_point->rgb[1] = point.g;
			laszip_point->rgb[2] = point.b;
			laszip_point->intensity = point.intensity;
			laszip_point->user_data = point.user_data;
			laszip_point->point_source_ID = point.point_source_ID;
			laszip_point->gps_time = point.gps_time;

			if (isExtended)
			{
				laszip_point->extended_return_number = point.return_number;
				laszip_point->extended_number_of_returns = point.number_of_returns;
				laszip_point->return_number = std::min<uint8_t>(point.return_number, 7);
				laszip_point->number_of_returns = std::min<uint8_t>(point.number_of_returns, 7);

				// laszip requires the legacy fields to be consistent with the extended ones.
				laszip_point->extended_classification = point.classification;
				laszip_point->classification = point.classification < 32 ? point.classification : 0;
				laszip_point->extended_classification_flags = point.classification_flags & 0x0F;
				laszip_point->synthetic_flag = (point.classification_flags >> 0) & 0x1;
				laszip_point->keypoint_flag = (point.classification_flags >> 1) & 0x1;
				laszip_point->withheld_flag = (point.classification_flags >> 2) & 0x1;

				laszip_point->extended_scan_angle = point.scan_angle;
			}
			else
			{
				laszip_point->return_number = point.return_number;
				laszip_point->number_of_returns = point.number_of_returns;
				laszip_point->classification = point.classification;
				laszip_point->scan_angle_rank = point.scan_angle_rank;
			}

			if (ph->extra_bytes > 0)
			{
				memcpy(laszip_point->extra_bytes, points_array->extra_bytes.data() + i * ph->extra_bytes, ph->extra_bytes);
			}

			ok = ok && checkLaszip(laszip_writer, laszip_set_coordinates(laszip_writer, coordinates), "setting coordinates");
			ok = ok && checkLaszip(laszip_writer, laszip_write_point(laszip_writer), "writing point");
			ok = ok && checkLaszip(laszip_writer, laszip_update_inventory(laszip_writer), "updating inventory");

			ph->number_of_points++;
		}

		delete points_array;

		auto now = std::chrono::steady_clock::now();
		if (now - lastReport > std::chrono::seconds(1) && ph->expected_number_of_points > 0)
		{
			lastReport = now;
			std::cout << "[" << std::round((100.0 * ph->number_of_points) / ph->expected_number_of_points) << "%] Written points: " << ph->number_of_points << std::endl;
		}
	}

	if (!ok)
	{
		drainPoints(ph);
	}

	std::cout << "Written points: " << ph->number_of_points << std::endl;

	ok = checkLaszip(laszip_writer, laszip_close_writer(laszip_writer), "closing las writer") && ok;
	laszip_destroy(laszip_writer);

	if (ph->expected_number_of_points != ph->number_of_points)
	{
		std::cout << "Warning: Number of written points doesn't equal expected number of points" << std::endl;
		std::cout << "Expected number of points: " << ph->expected_number_of_points << std::endl;
		std::cout << "Number of points written to file: " << ph->number_of_points << std::endl;
	}

	*success = ok;
}

bool parallel_handler::createFillPointsThreads()
{
	void (*fillPoints)(parallel_handler*) = nullptr;

	if (this->version == "2.0")
	{
		if (this->encoding == "DEFAULT")
		{
			fillPoints = fillPointsDEFAULT;
			std::cout << "Activating PotreeToLas for DEFAULT encoding!" << std::endl;
		}
		else if (this->encoding == "BROTLI")
		{
			fillPoints = fillPointsBROTLI;
			std::cout << "Activating PotreeToLas for BROTLI encoding!" << std::endl;
		}
		else
		{
			std::cout << "ERROR: encoding " << this->encoding << " is not supported, only DEFAULT and BROTLI." << std::endl;
			return false;
		}
	}
	else if (this->version == "1.7")
	{
		fillPoints = fillPointsDEFAULT17;
		std::cout << "Activating PotreeToLas for PotreeConverter 1.7!" << std::endl;
	}

	int number_of_threads = std::max<int>(1, getCpuData().numProcessors);
	std::cout << "Using " << number_of_threads << " threads for points reading." << std::endl;

	bool success = false;
	std::thread savePoints(&save, this, &success);

	std::vector<std::thread> fillPointsThreads;
	for (int i = 0; i < number_of_threads; i++)
	{
		fillPointsThreads.emplace_back(fillPoints, this);
	}

	for (auto& thread : fillPointsThreads)
	{
		thread.join();
	}

	this->points_queue->push(nullptr);
	savePoints.join();
	end_writing = true;

	return success;
}
