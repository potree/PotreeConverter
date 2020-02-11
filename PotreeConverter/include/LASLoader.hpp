
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

#include "Points.h"
#include "stuff.h"
#include "Vector3.h"
//
//using std::string;
//using std::cout;
//using std::endl;
//using std::vector;
//using std::unordered_map;
//using std::thread;
//using std::future;
//using std::mutex;
//using std::lock_guard;
//using std::unique_lock;

using namespace std;

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
	{0, ExtraType{AttributeType::undefined, 0, 1}},
	{1, ExtraType{AttributeType::uint8, 1, 1}},
	{2, ExtraType{AttributeType::int8, 1, 1}},
	{3, ExtraType{AttributeType::uint16, 2, 1}},
	{4, ExtraType{AttributeType::int16, 2, 1}},
	{5, ExtraType{AttributeType::uint32, 4, 1}},
	{6, ExtraType{AttributeType::int32, 4, 1}},
	{7, ExtraType{AttributeType::uint64, 8, 1}},
	{8, ExtraType{AttributeType::int64, 8, 1}},
	{9, ExtraType{AttributeType::float32, 4, 1}},
	{10, ExtraType{AttributeType::float64, 8, 1}},
};


Attributes estimateAttributes(string path) {
	laszip_POINTER laszip_reader = nullptr;

	laszip_create(&laszip_reader);

	laszip_BOOL request_reader = 1;
	laszip_request_compatibility_mode(laszip_reader, request_reader);

	laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
	laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

	laszip_header* header = nullptr;
	laszip_get_header_pointer(laszip_reader, &header);

	int64_t npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

	//int64_t nPointsChecking = std::min(npoints, 1'000'000ll);

	//for (int64_t i = 0; i < nPointsChecking; i++) {

	//}

	Attributes attributes;

	{ // read extra bytes

		for (int i = 0; i < header->number_of_variable_length_records; i++) {
			laszip_vlr_struct vlr = header->vlrs[i];

			if (vlr.record_id != 4) {
				continue;
			}

			cout << "record id: " << vlr.record_id << endl;
			cout << "record_length_after_header: " << vlr.record_length_after_header << endl;

			int numExtraBytes = vlr.record_length_after_header / sizeof(ExtraBytesRecord);

			ExtraBytesRecord* extraBytes = reinterpret_cast<ExtraBytesRecord*>(vlr.data);

			for (int j = 0; j < numExtraBytes; j++) {
				ExtraBytesRecord extraAttribute = extraBytes[j];

				string name = string(extraAttribute.name);

				cout << "name: " << name << endl;

				ExtraType et = typeToExtraType.at(extraAttribute.data_type);
				
				Attribute attribute(name, et.type);
				attribute.bytes = et.size;
				attribute.numElements = et.numElements;

				attributes.add(attribute);

			}
		}
	}

	return attributes;
}


class LASLoader {

public:

	laszip_POINTER laszip_reader = nullptr;
	laszip_header* header = nullptr;
	laszip_point* point;

	uint64_t batchSize = 1'000'000;
	vector<shared_ptr<Points>> batches;
	bool finishedLoading = false;

	uint64_t numPoints = 0;
	Vector3<double> min = { 0.0, 0.0, 0.0 };
	Vector3<double> max = { 0.0, 0.0, 0.0 };


	mutex mtx_batches;
	mutex mtx_finishedLoading;

	LASLoader(string path) {

		laszip_create(&laszip_reader);

		laszip_BOOL request_reader = 1;
		laszip_request_compatibility_mode(laszip_reader, request_reader);

		laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
		laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

		laszip_get_header_pointer(laszip_reader, &header);

		this->min = {header->min_x, header->min_y, header->min_z};
		this->max = {header->max_x, header->max_y, header->max_z};

		uint64_t npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

		this->numPoints = npoints;
		
		//spawnLoadThread();

	}

	Attributes getAttributes() {
		Attributes attributes;
		Attribute aColor("color", AttributeType::uint8);
		aColor.byteOffset = 12;
		aColor.bytes = 4;

		attributes.list.push_back(aColor);
		attributes.byteSize += aColor.bytes;

		return attributes;
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

	void loadStuff() {

		auto tStart = now();

		uint64_t npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

		laszip_get_point_pointer(laszip_reader, &point);

		shared_ptr<Points> points;

		double coordinates[3];

		Attributes attributes = getAttributes();

		for (uint64_t i = 0; i < npoints; i++) {

			if ((i % batchSize) == 0) {

				if (points != nullptr) {
					lock_guard<mutex> guard(mtx_batches);

					batches.push_back(points);
				}

				uint64_t currentBatchSize = std::min(npoints - i, batchSize);

				points = make_shared<Points>();
				uint64_t attributeBufferSize = currentBatchSize * attributes.byteSize;
				points->points.reserve(currentBatchSize);
				points->attributes = attributes;
				points->attributeBuffer = make_shared<Buffer>(attributeBufferSize);
			}

			laszip_read_point(laszip_reader);

			uint8_t r = point->rgb[0] / 256;
			uint8_t g = point->rgb[1] / 256;
			uint8_t b = point->rgb[2] / 256;

			laszip_get_coordinates(laszip_reader, coordinates);

			uint64_t reli = i % batchSize;

			Point point = {
				coordinates[0],
				coordinates[1],
				coordinates[2],
				reli
			};
			points->points.push_back(point);

			uint8_t* rgbBuffer = points->attributeBuffer->dataU8 + (4 * reli + 0);

			rgbBuffer[0] = r;
			rgbBuffer[1] = g;
			rgbBuffer[2] = b;
			rgbBuffer[3] = 255;
		}

		{
			lock_guard<mutex> guard1(mtx_batches);
			lock_guard<mutex> guard2(mtx_finishedLoading);

			batches.push_back(points);
			finishedLoading = true;
		}

		cout << "#points: " << npoints << endl;
		printElapsedTime("loaded", tStart);

		cout << batches.size() << endl;
	}

	void spawnLoadThread() {
	
		thread t([&](){
			loadStuff();
		});
		t.detach();

	}

};