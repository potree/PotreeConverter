
#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <future>
#include <experimental/coroutine>

#include "laszip_api.h"

#include "stuff.h"

using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::thread;
using std::future;
using std::mutex;
using std::lock_guard;
using std::unique_lock;

struct Buffer {
	void* data = nullptr;
	uint8_t* dataU8 = nullptr;
	uint16_t* dataU16 = nullptr;
	uint32_t* dataU32 = nullptr;
	int8_t* dataI8 = nullptr;
	int16_t* dataI16 = nullptr;
	int32_t* dataI32 = nullptr;
	float* dataF = nullptr;
	double* dataD = nullptr;

	uint64_t size = 0;

	Buffer(uint64_t size) {

		this->data = malloc(size);

		this->dataU8 = reinterpret_cast<uint8_t*>(this->data);
		this->dataU16 = reinterpret_cast<uint16_t*>(this->data);
		this->dataU32 = reinterpret_cast<uint32_t*>(this->data);
		this->dataI8 = reinterpret_cast<int8_t*>(this->data);
		this->dataI16 = reinterpret_cast<int16_t*>(this->data);
		this->dataI32 = reinterpret_cast<int32_t*>(this->data);
		this->dataF = reinterpret_cast<float*>(this->data);
		this->dataD = reinterpret_cast<double*>(this->data);

		this->size = size;
	}

	~Buffer() {
		free(this->data);
	}

};

struct Attribute {

	string name = "undefined";
	Buffer* data = nullptr;
	uint64_t byteOffset = 0;
	uint64_t bytes = 0;

	Attribute() {

	}

};

struct Points {

	vector<Attribute> attributes;

};

class LASLoader {

public:

	laszip_POINTER laszip_reader = nullptr;
	laszip_header* header = nullptr;
	laszip_point* point;

	uint64_t batchSize = 1'000'000;
	vector<Points*> batches;
	bool finishedLoading = false;

	mutex mtx_batches;
	mutex mtx_finishedLoading;

	LASLoader(string path) {

		laszip_create(&laszip_reader);

		laszip_BOOL request_reader = 1;
		laszip_request_compatibility_mode(laszip_reader, request_reader);

		laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
		laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

		laszip_get_header_pointer(laszip_reader, &header);

		
		spawnLoadThread();

	}

	future<Points*> nextBatch() {

		auto fut = std::async(std::launch::async, [=]() -> Points* {

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
					Points* batch = batches.back();
					batches.pop_back();

					lock.unlock();

					return batch;
				} else {
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}

			cout << "damn" << endl;
				
			return nullptr;
		});

		return fut;
	}

	void loadStuff() {

		uint64_t npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

		laszip_get_point_pointer(laszip_reader, &point);

		Points* points = nullptr;

		double coordinates[3];

		for (uint64_t i = 0; i < npoints; i++) {

			if ((i % batchSize) == 0) {

				if (points != nullptr) {
					lock_guard<mutex> guard(mtx_batches);

					batches.push_back(points);
				}

				Attribute aPosition;
				aPosition.byteOffset = 0;
				aPosition.bytes = 12;
				aPosition.name = "position";
				aPosition.data = new Buffer(batchSize * aPosition.bytes);

				Attribute aColor;
				aColor.byteOffset = 12;
				aColor.bytes = 4;
				aColor.name = "color";
				aColor.data = new Buffer(batchSize * aColor.bytes);

				points = new Points();
				points->attributes.push_back(aPosition);
				points->attributes.push_back(aColor);
			}

			laszip_read_point(laszip_reader);

			uint8_t r = point->rgb[0] / 255;
			uint8_t g = point->rgb[1] / 255;
			uint8_t b = point->rgb[2] / 255;

			laszip_get_coordinates(laszip_reader, coordinates);

			uint64_t reli = i % batchSize;

			points->attributes[0].data->dataU32[3 * reli + 0] = point->X;
			points->attributes[0].data->dataU32[3 * reli + 1] = point->Y;
			points->attributes[0].data->dataU32[3 * reli + 2] = point->Z;

			points->attributes[1].data->dataU8[4 * reli + 0] = r;
			points->attributes[1].data->dataU8[4 * reli + 1] = g;
			points->attributes[1].data->dataU8[4 * reli + 2] = b;
			points->attributes[1].data->dataU8[4 * reli + 3] = 0;
		}

		{
			lock_guard<mutex> guard(mtx_batches);

			batches.push_back(points);
		}

		{
			lock_guard<mutex> guard(mtx_finishedLoading);

			finishedLoading = true;
		}
		

		cout << "#points: " << npoints << endl;

		cout << batches.size() << endl;
	}

	void spawnLoadThread() {
	
		thread t([&](){
			loadStuff();
		});
		t.detach();

	}

};