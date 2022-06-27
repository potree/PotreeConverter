
#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>

//#include "LasLoader/LasLoader.h"
#include "unsuck/unsuck.hpp"
#include "Vector3.h"

using std::ios;
using std::thread;
using std::mutex;
using std::lock_guard;
using std::atomic_int64_t;

namespace fs = std::filesystem;

struct LASPointF2 {
	int32_t x;
	int32_t y;
	int32_t z;
	uint16_t intensity;
	uint8_t returnNumber;
	uint8_t classification;
	uint8_t scanAngleRank;
	uint8_t userData;
	uint16_t pointSourceID;
	uint16_t r;
	uint16_t g;
	uint16_t b;
};


struct Source {
	string path;
	uint64_t filesize;

	uint64_t numPoints = 0;
	int bytesPerPoint = 0;
	Vector3 min;
	Vector3 max;
};

struct State {
	string name = "";
	atomic_int64_t pointsTotal = 0;
	atomic_int64_t pointsProcessed = 0;
	atomic_int64_t bytesProcessed = 0;
	double duration = 0.0;
	std::map<string, string> values;

	int numPasses = 3;
	int currentPass = 0; // starts with index 1! interval: [1,  numPasses]

	mutex mtx;

	double progress() {
		return double(pointsProcessed) / double(pointsTotal);
	}
};


struct BoundingBox {
	Vector3 min;
	Vector3 max;

	BoundingBox() {
		this->min = { Infinity,Infinity,Infinity };
		this->max = { -Infinity,-Infinity,-Infinity };
	}

	BoundingBox(Vector3 min, Vector3 max) {
		this->min = min;
		this->max = max;
	}
};


// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
// method to seperate bits from a given integer 3 positions apart
inline uint64_t splitBy3(unsigned int a) {
	uint64_t x = a & 0x1fffff; // we only look at the first 21 bits
	x = (x | x << 32) & 0x1f00000000ffff; // shift left 32 bits, OR with self, and 00011111000000000000000000000000000000001111111111111111
	x = (x | x << 16) & 0x1f0000ff0000ff; // shift left 32 bits, OR with self, and 00011111000000000000000011111111000000000000000011111111
	x = (x | x << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, and 0001000000001111000000001111000000001111000000001111000000000000
	x = (x | x << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, and 0001000011000011000011000011000011000011000011000011000100000000
	x = (x | x << 2) & 0x1249249249249249;
	return x;
}

// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
inline uint64_t mortonEncode_magicbits(unsigned int x, unsigned int y, unsigned int z) {
	uint64_t answer = 0;
	answer |= splitBy3(x) | splitBy3(y) << 1 | splitBy3(z) << 2;
	return answer;
}

inline BoundingBox childBoundingBoxOf(Vector3 min, Vector3 max, int index) {
	BoundingBox box;
	auto size = max - min;
	Vector3 center = min + (size * 0.5);

	if ((index & 0b100) == 0) {
		box.min.x = min.x;
		box.max.x = center.x;
	} else {
		box.min.x = center.x;
		box.max.x = max.x;
	}

	if ((index & 0b010) == 0) {
		box.min.y = min.y;
		box.max.y = center.y;
	} else {
		box.min.y = center.y;
		box.max.y = max.y;
	}

	if ((index & 0b001) == 0) {
		box.min.z = min.z;
		box.max.z = center.z;
	} else {
		box.min.z = center.z;
		box.max.z = max.z;
	}

	return box;
}

inline void dbgPrint_ts_later(string message, bool now = false) {
	
	static vector<string> data;

	static mutex mtx;
	lock_guard<mutex> lock(mtx);

	data.push_back(message);

	if (now) {
		for (auto& message : data) {
			cout << message << endl;
		}

		data.clear();
	} 

	
}


struct Options {
	vector<string> source;
	string encoding = "DEFAULT"; // "BROTLI", "UNCOMPRESSED"
	string outdir = "";
	string name = "";
	string method = "";
	string chunkMethod = "";
	//vector<string> flags;
	vector<string> attributes;
	bool generatePage = false;
	string pageName = "";
	string pageTitle = "";

	bool keepChunks = false;
	bool noChunking = false;
	bool noIndexing = false;

	int numThreads = 0;

};