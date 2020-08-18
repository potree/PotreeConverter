
#pragma once

#include "laszip/laszip_api.h"


#include "unsuck/unsuck.hpp"
#include "Vector3.h"



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

LasHeader loadLasHeader(string path) {
	laszip_POINTER laszip_reader;
	laszip_header* header;
	laszip_point* point;

	laszip_BOOL request_reader = 1;
	laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;

	laszip_create(&laszip_reader);
	laszip_request_compatibility_mode(laszip_reader, request_reader);
	laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);
	laszip_get_header_pointer(laszip_reader, &header);

	LasHeader result;

	result.min.x = header->min_x;
	result.min.y = header->min_y;
	result.min.z = header->min_z;

	result.max.x = header->max_x;
	result.max.y = header->max_y;
	result.max.z = header->max_z;

	result.scale.x = header->x_scale_factor;
	result.scale.y = header->y_scale_factor;
	result.scale.z = header->z_scale_factor;

	result.offset.x = header->x_offset;
	result.offset.y = header->y_offset;
	result.offset.z = header->z_offset;

	result.numPoints = std::max(header->extended_number_of_point_records, uint64_t(header->number_of_point_records));

	result.pointDataFormat = header->point_data_format;

	int numVlrs = header->number_of_variable_length_records;
	for (int i = 0; i < numVlrs; i++) {
		auto laszip_vlr = header->vlrs[i];

		VLR vlr;

		vlr.recordID = laszip_vlr.record_id;
		vlr.recordLengthAfterHeader = laszip_vlr.record_length_after_header;
		vlr.data.resize(vlr.recordLengthAfterHeader);

		memcpy(vlr.data.data(), laszip_vlr.data, vlr.recordLengthAfterHeader);

		result.vlrs.push_back(vlr);
	}

	


	laszip_close_reader(laszip_reader);
	laszip_destroy(laszip_reader);

	return result;
}



