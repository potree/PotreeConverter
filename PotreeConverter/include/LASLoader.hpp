
#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <future>
#include <experimental/coroutine>
#include <algorithm>
#include <cmath>

#include "laszip_api.h"


#include "LASWriter.hpp"
#include "Points.h"
#include "stuff.h"
#include "Vector3.h"

using namespace std;


inline Attributes LAS_FORMAT_0({
	{"position",        AttributeTypes::int32, 3},
	{"intensity",       AttributeTypes::uint16},
	{"returns",         AttributeTypes::uint8}, 
	{"classification",  AttributeTypes::uint8},
	{"scan angle rank", AttributeTypes::int8},
	{"user data",       AttributeTypes::uint8},
	{"point source id", AttributeTypes::uint16},
});

inline Attributes LAS_FORMAT_1({
	{"position",        AttributeTypes::int32, 3},
	{"intensity",       AttributeTypes::uint16},
	{"returns",         AttributeTypes::uint8},
	{"classification",  AttributeTypes::uint8},
	{"scan angle rank", AttributeTypes::int8},
	{"user data",       AttributeTypes::uint8},
	{"point source id", AttributeTypes::uint16},
	{"gps-time",        AttributeTypes::float64},
});

inline Attributes LAS_FORMAT_2({
	{"position",        AttributeTypes::int32, 3},
	{"intensity",       AttributeTypes::uint16},
	{"returns",         AttributeTypes::uint8},
	{"classification",  AttributeTypes::uint8},
	{"scan angle rank", AttributeTypes::int8},
	{"user data",       AttributeTypes::uint8},
	{"point source id", AttributeTypes::uint16},
	{"rgb",             AttributeTypes::uint16, 3},
});

inline Attributes LAS_FORMAT_3({
	{"position",        AttributeTypes::int32, 3},
	{"intensity",       AttributeTypes::uint16},
	{"returns",         AttributeTypes::uint8},
	{"classification",  AttributeTypes::uint8},
	{"scan angle rank", AttributeTypes::int8},
	{"user data",       AttributeTypes::uint8},
	{"point source id", AttributeTypes::uint16},
	{"gps-time",        AttributeTypes::float64},
	{"rgb",             AttributeTypes::uint16, 3},
});

inline Attributes LAS_FORMAT_4({
	{"position",        AttributeTypes::int32, 3},
	{"intensity",       AttributeTypes::uint16},
	{"returns",         AttributeTypes::uint8},
	{"classification",  AttributeTypes::uint8},
	{"scan angle rank", AttributeTypes::int8},
	{"user data",       AttributeTypes::uint8},
	{"point source id", AttributeTypes::uint16},
	{"gps-time",        AttributeTypes::float64},
	{"wave packet descriptor index",   AttributeTypes::uint8},
	{"byte offset to waveform data",   AttributeTypes::uint64},
	{"waveform packet size",           AttributeTypes::uint32},
	{"return point waveform location", AttributeTypes::float32},
	{"XYZ(t)",          AttributeTypes::float32, 3},
});

inline vector<Attributes> lasFormats = {
	LAS_FORMAT_0,
	LAS_FORMAT_1,
	LAS_FORMAT_2,
	LAS_FORMAT_3,
	LAS_FORMAT_4
};


// see LAS spec 1.4
// https://www.asprs.org/wp-content/uploads/2010/12/LAS_1_4_r13.pdf
// total of 192 bytes 
struct ExtraBytesRecord {
	unsigned char reserved[2];
	unsigned char data_type;
	unsigned char options;
	char name[32];
	unsigned char unused[4];
	int64_t no_data[3]; // 24 = 3*8 bytes // hack: not really int, can be double too
	int64_t min[3]; // 24 = 3*8 bytes	  // hack: not really int, can be double too
	int64_t max[3]; // 24 = 3*8 bytes	  // hack: not really int, can be double too
	double scale[3];
	double offset[3];
	char description[32];
};

struct ExtraType {
	AttributeType type;
	int size = 0;
	int numElements = 0;
};

const unordered_map<unsigned char, ExtraType> typeToExtraType = {
	{0, ExtraType{AttributeTypes::undefined, 0, 1}},
	{1, ExtraType{AttributeTypes::uint8, 1, 1}},
	{2, ExtraType{AttributeTypes::int8, 1, 1}},
	{3, ExtraType{AttributeTypes::uint16, 2, 1}},
	{4, ExtraType{AttributeTypes::int16, 2, 1}},
	{5, ExtraType{AttributeTypes::uint32, 4, 1}},
	{6, ExtraType{AttributeTypes::int32, 4, 1}},
	{7, ExtraType{AttributeTypes::uint64, 8, 1}},
	{8, ExtraType{AttributeTypes::int64, 8, 1}},
	{9, ExtraType{AttributeTypes::float32, 4, 1}},
	{10, ExtraType{AttributeTypes::float64, 8, 1}},
};

//struct LasPointMapping {
//	int offset = 0;
//	int size = 0;
//};

//inline unordered_map<string, LasPointMapping> laspointMappings = {
//	{"position",  {offsetof(laszip_point, X), 12}},
//	{"intensity", {offsetof(laszip_point, intensity), sizeof(laszip_point_struct::intensity)}},
//	{"returns", {offsetof(laszip_point, intensity), sizeof(laszip_point_struct::intensity)}},
//};

//inline int abc = sizeof(laszip_point_struct::intensity);

inline Attributes estimateAttributes(string path) {
	laszip_POINTER laszip_reader = nullptr;

	laszip_create(&laszip_reader);

	laszip_BOOL request_reader = 1;
	laszip_request_compatibility_mode(laszip_reader, request_reader);

	laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
	laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

	laszip_header* header = nullptr;
	laszip_get_header_pointer(laszip_reader, &header);

	laszip_point* point;
	laszip_get_point_pointer(laszip_reader, &point);

	int64_t npoints = header->number_of_point_records ? 
		header->number_of_point_records : 
		header->extended_number_of_point_records;

	auto format = header->point_data_format;
	
	if (format > lasFormats.size()) {
		cout << "ERROR: unsupported las format: " << format << endl;
		exit(123);
	}
	
	auto attributes = lasFormats[format];


	int numPointsToCheck = std::min(npoints, 100'000ll);
	//int numPointsToCheck = std::min(npoints, 10ll);

	vector<int> nonemptyCounter(attributes.list.size());

	for (int i = 0; i < numPointsToCheck; i++) {
		// point
		laszip_read_point(laszip_reader);
		
		int j = 0;
		for (auto attribute : attributes.list) {

			if (attribute.name == "position") {
				nonemptyCounter[j]++;
			} else if (attribute.name == "intensity") {
				if (point->intensity != 0) {
					nonemptyCounter[j]++;
				}
			} else if (attribute.name == "returns") {
				if (point->return_number != 0) {
					nonemptyCounter[j]++;
				} else if (point->number_of_returns != 0) {

				}
			} else if (attribute.name == "classification") {
				if (point->classification != 0) {
					nonemptyCounter[j]++;
				}
			} else if (attribute.name == "point source id") {
				if (point->point_source_ID != 0) {
					nonemptyCounter[j]++;
				}
			} else if (attribute.name == "gps-time") {
				if (point->gps_time != 0.0) {
					nonemptyCounter[j]++;
				}
			} else if (attribute.name == "rgb") {
				bool hasRGB = point->rgb[0] != 0
					|| point->rgb[1] != 0
					|| point->rgb[2] != 0;
				if (point->rgb[0] != 0) {
					nonemptyCounter[j]++;
				}
			}
			
			j++;
		}
	}

	vector<Attribute> nonemptyAttributes;
	for (int i = 0; i < attributes.list.size(); i++) {
		Attribute attribute = attributes.list[i];

		if (nonemptyCounter[i] > 0) {
			nonemptyAttributes.push_back(attribute);
		}
	}

	//vector<laszip_point> laszipPoints(numPointsToCheck);

	//for (int i = 0; i < numPointsToCheck; i++) {
	//	laszip_read_point(laszip_reader);

	//	laszipPoints.push_back(*point);
	//}

	//for (auto attribute : attributes.list) {

	//	if (attribute.name == "position") {
	//		nonemptyAttributes.push_back(attribute);
	//	} else if (attribute.name == "intensity") {
	//		for (int i = 0; i < numPointsToCheck; i++) {
	//			if (laszipPoints[i].intensity != 0) {
	//				nonemptyAttributes.push_back(attribute);
	//				continue;
	//			}
	//		}


	//	}

	//}


	//{ // TODO: read extra bytes

	//	for (uint64_t i = 0; i < header->number_of_variable_length_records; i++) {
	//		laszip_vlr_struct vlr = header->vlrs[i];

	//		if (vlr.record_id != 4) {
	//			continue;
	//		}

	//		cout << "record id: " << vlr.record_id << endl;
	//		cout << "record_length_after_header: " << vlr.record_length_after_header << endl;

	//		int numExtraBytes = vlr.record_length_after_header / sizeof(ExtraBytesRecord);

	//		ExtraBytesRecord* extraBytes = reinterpret_cast<ExtraBytesRecord*>(vlr.data);

	//		for (int j = 0; j < numExtraBytes; j++) {
	//			ExtraBytesRecord extraAttribute = extraBytes[j];

	//			string name = string(extraAttribute.name);

	//			cout << "name: " << name << endl;

	//			ExtraType et = typeToExtraType.at(extraAttribute.data_type);
	//			
	//			Attribute attribute(name, et.type);
	//			attribute.bytes = et.size;
	//			attribute.numElements = et.numElements;

	//			attributes.add(attribute);

	//		}
	//	}
	//}

	return nonemptyAttributes;
}

struct LasLoadTask {
	uint64_t start = 0;
	uint64_t numPoints = 0;
	bool done = false;
};

class LASLoader {

public:

	//laszip_POINTER laszip_reader = nullptr;
	//laszip_header* header = nullptr;
	//laszip_point* point;

	uint64_t batchSize = 1'000'000;
	vector<shared_ptr<Points>> batches;
	bool finishedLoading = false;

	uint64_t numPoints = 0;
	Vector3<double> min = { 0.0, 0.0, 0.0 };
	Vector3<double> max = { 0.0, 0.0, 0.0 };

	Attributes attributes;

	mutex mtx_batches;
	mutex mtx_finishedLoading;
	mutex mtx_loadTask;

	string path;
	int numThreads = 0;
	int nextBatchStart = 0;
	int activeThreads = 0;

	LASLoader(string path, int numThreads) {

		this->path = path;
		this->numThreads = numThreads;
		

		laszip_POINTER laszip_reader = nullptr;
		laszip_header* header = nullptr;

		laszip_create(&laszip_reader);

		laszip_BOOL request_reader = 1;
		laszip_request_compatibility_mode(laszip_reader, request_reader);

		laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
		laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

		laszip_get_header_pointer(laszip_reader, &header);

		this->min = {header->min_x, header->min_y, header->min_z};
		this->max = {header->max_x, header->max_y, header->max_z};

		uint64_t npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);
		//uint64_t npoints = 10'000;

		this->numPoints = npoints;

		laszip_close_reader(laszip_reader);

		this->attributes = estimateAttributes(path);

		spawnThreads();
	}

	LasLoadTask getLoadTask() {
		lock_guard<mutex> lock(mtx_loadTask);

		LasLoadTask task;
		task.start = nextBatchStart;		
		task.numPoints = std::min(task.start + batchSize, numPoints) - task.start;

		if (nextBatchStart >= numPoints) {
			task.done = true;
		}

		nextBatchStart += task.numPoints;

		return task;
	}

	Attributes getAttributes() {
		return this->attributes;
	}

	future<shared_ptr<Points>> nextBatch() {

		auto fut = std::async(std::launch::async, [=]() -> shared_ptr<Points> {

			bool done = false;

			while (!done) {
				{
					lock_guard<mutex> guard1(mtx_finishedLoading);
					lock_guard<mutex> guard2(mtx_batches);

					bool nothingLeftTodo = finishedLoading && batches.size() == 0;

					if (nothingLeftTodo) {
						return nullptr;
					}
				}

				//unique<mutex> guard(mtx_batches);
				unique_lock<mutex> lock(mtx_batches, std::defer_lock);
				lock.lock();
				if (batches.size() > 0) {
					auto batch = batches.back();
					batches.pop_back();

					lock.unlock();

					return batch;
				} else {
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}
			}

			cout << "damn" << endl;
				
			return nullptr;
		});

		return fut;
	}

	void spawnThreads() {
		this->activeThreads = numThreads;

		for (int i = 0; i < numThreads; i++) {
			spawnLoadThread();
		}

	}

	void spawnLoadThread() {

		thread t([this]() {

			laszip_POINTER laszip_reader;

			laszip_create(&laszip_reader);

			laszip_BOOL request_reader = 1;
			laszip_request_compatibility_mode(laszip_reader, request_reader);

			laszip_BOOL is_compressed = iEndsWith(this->path, ".laz") ? 1 : 0;
			laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

			laszip_header* header = nullptr;
			laszip_get_header_pointer(laszip_reader, &header);

			laszip_point* ptrPoint;
			laszip_get_point_pointer(laszip_reader, &ptrPoint);

			Attributes attributes = this->attributes;


			LasLoadTask task = getLoadTask();

			while (!task.done) {

				uint64_t end = task.start + task.numPoints;
				laszip_seek_point(laszip_reader, task.start);

				double coordinates[3];

				shared_ptr<Points> points = make_shared<Points>();
				points->points.reserve(task.numPoints);
				points->attributes = attributes;
				uint64_t attributeBufferSize = task.numPoints * attributes.byteSize;
				auto buffer = make_shared<Buffer>(attributeBufferSize);
				points->attributeBuffer = buffer;

				vector<function<void(int)>> attributeReaders;

				int offset = 0;
				for (Attribute attribute : attributes.list) {

					if (attribute.name == "position") {
						// handle position separately
					} else if (attribute.name == "rgb") {
						int attributeOffset = offset;
						auto reader = [ptrPoint, buffer, points, attributes, attributeOffset](int i) {

							uint8_t* pointer = buffer->dataU8 + (attributes.byteSize * i + attributeOffset);
							uint16_t* rgbBuffer = reinterpret_cast<uint16_t*>(pointer);

							rgbBuffer[0] = ptrPoint->rgb[0];
							rgbBuffer[1] = ptrPoint->rgb[1];
							rgbBuffer[2] = ptrPoint->rgb[2];
						};
						attributeReaders.push_back(reader);
					}

					offset += attribute.bytes;
					
				}
				


				int relIndex = 0;
				for (uint64_t i = task.start; i < end; i++) {
					laszip_read_point(laszip_reader);

					laszip_get_coordinates(laszip_reader, coordinates);

					Point point = {
						coordinates[0],
						coordinates[1],
						coordinates[2],
						relIndex
					};

					//uint8_t r = ptrPoint->rgb[0] / 256;
					//uint8_t g = ptrPoint->rgb[1] / 256;
					//uint8_t b = ptrPoint->rgb[2] / 256;

					//uint8_t* rgbBuffer = points->attributeBuffer->dataU8 + (attributes.byteSize * relIndex + 24);

					//rgbBuffer[0] = r;
					//rgbBuffer[1] = g;
					//rgbBuffer[2] = b;
					//rgbBuffer[3] = 255;

					// store coordinates in attribute buffer
					auto target = buffer->dataU8 + attributes.byteSize * relIndex;
					auto source = coordinates;
					memcpy(target, source, 12);

					// store all remaining attributes in attribute buffer
					for(auto& attributeReader : attributeReaders){
						attributeReader(relIndex);
					}

					points->points.push_back(point);

					relIndex++;
				}

				addLoadedBatch(points);

				task = getLoadTask();
			}


			{
				lock_guard<mutex> guard2(mtx_finishedLoading);

				activeThreads--;

				if (activeThreads == 0) {
					finishedLoading = true;
				}
			}




		});

		t.detach();
	}


	private:

	void addLoadedBatch(shared_ptr<Points> points) {
		lock_guard<mutex> guard(mtx_batches);

		batches.push_back(points);
	}

};