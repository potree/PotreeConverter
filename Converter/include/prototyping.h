
#pragma once

#include <sstream>
#include <string>
#include <thread>
#include <execution>
#include <algorithm>
#include <random>
#include <algorithm>


#include "LasLoader/LasLoader.h"
#include "unsuck/unsuck.hpp"
#include "converter_utils.h"
#include "Vector3.h"

vector<Vector3> createData(int64_t n) {

	vector<Vector3> points;
	points.reserve(n);
	
	double sqn = sqrt(n);
	for (int64_t i = 0; i < n; i++) {
		double u = fmod(double(i), sqn) / sqn;
		double v = double(i) / double(n);

		double x = 2.0 * u - 1.0;
		double y = 2.0 * v - 1.0;
		double z = cos(10.0 * u) * cos(10.0 * v);

		Vector3 point(x, y, z);

		points.push_back(point);

	}

	return points;
}

void countWithVector(double gridSize, vector<Vector3>& points, Vector3 min, Vector3 max) {
	vector<int32_t> counters(gridSize * gridSize * gridSize, 0);
	Vector3 size = max - min;

	int64_t prevIndex = 0;
	int32_t count = 0;

	int numCells = 0;
	for (Vector3 point : points) {

		double dx = gridSize * (point.x - min.x) / size.x;
		double dy = gridSize * (point.y - min.y) / size.y;
		double dz = gridSize * (point.z - min.z) / size.z;

		int64_t ix = std::min(dx, gridSize - 1.0);
		int64_t iy = std::min(dy, gridSize - 1.0);
		int64_t iz = std::min(dz, gridSize - 1.0);

		int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

		count++;

		if (prevIndex != index) {
			int oldCounter = counters[index];
			counters[index] += count;
			count = 0;


			if (oldCounter == 0) {
				numCells++;
			}
		}

		prevIndex = index;

		//int oldCounter = counters[index]++;
		//if (oldCounter == 0) {
		//	numCells++;
		//}
	}

	cout << "#cells: " << numCells << endl;
}

void countWithUnorderedMap(double gridSize, vector<Vector3>& points, Vector3 min, Vector3 max) {
	//vector<int32_t> counters(gridSize * gridSize * gridSize, 0);
	unordered_map<int64_t, int32_t> counters;
	counters.reserve(5'000'000);
	Vector3 size = max - min;

	int64_t prevIndex = 0;
	int32_t count = 0;

	int numCells = 0;
	for (Vector3 point : points) {

		double dx = gridSize * (point.x - min.x) / size.x;
		double dy = gridSize * (point.y - min.y) / size.y;
		double dz = gridSize * (point.z - min.z) / size.z;

		int64_t ix = std::min(dx, gridSize - 1.0);
		int64_t iy = std::min(dy, gridSize - 1.0);
		int64_t iz = std::min(dz, gridSize - 1.0);

		int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

		count++;

		if (prevIndex != index) {
			int oldCounter = counters[index];
			counters[index] += count;
			count = 0;


			if (oldCounter == 0) {
				numCells++;
			}
		}

		prevIndex = index;
		
	}
	cout << "#cells: " << numCells << endl;
}


void countWithCustomMap(double gridSize, vector<Vector3>& points, Vector3 min, Vector3 max) {
	//vector<int32_t> counters(gridSize * gridSize * gridSize, 0);

	struct Entry {
		int64_t index = -1;
		int32_t counter = 0;
	};

	int64_t P = 9'999'991;

	vector<Entry> counters(10'000'000);
	Vector3 size = max - min;

	int numCells = 0;
	for (Vector3 point : points) {

		double dx = gridSize * (point.x - min.x) / size.x;
		double dy = gridSize * (point.y - min.y) / size.y;
		double dz = gridSize * (point.z - min.z) / size.z;

		int64_t ix = std::min(dx, gridSize - 1.0);
		int64_t iy = std::min(dy, gridSize - 1.0);
		int64_t iz = std::min(dz, gridSize - 1.0);

		int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

		int64_t hash = index % P;

		Entry* entry = &counters[hash];

		if (entry->index == -1) {
			entry->index = index;
			numCells++;
		} else if (entry->index == index) {

		} else {
			for (int i = 0; i < 10; i++) {
				entry = &counters[hash + i];

				if (entry->index == -1 ) {
					entry->index = index;
					numCells++;

					break;
				} else if (entry->index == index) {
					break;
				}

			}
		}

		entry->counter++;


	}
	cout << "#cells: " << numCells << endl;
}

void countWithTreeStructure(double gridSize, vector<Vector3>& points, Vector3 min, Vector3 max) {
	
	struct Node {
		int32_t counter = 0;
		int32_t children[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	};

	vector<Node> nodes(1'000'000);

	Vector3 size = max - min;

	int numCells = 0;
	for (Vector3 point : points) {

		double dx = gridSize * (point.x - min.x) / size.x;
		double dy = gridSize * (point.y - min.y) / size.y;
		double dz = gridSize * (point.z - min.z) / size.z;

		int64_t ix = std::min(dx, gridSize - 1.0);
		int64_t iy = std::min(dy, gridSize - 1.0);
		int64_t iz = std::min(dz, gridSize - 1.0);

		int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;






	}
	cout << "#cells: " << numCells << endl;
}

void benchmarkHashMap() {

	int64_t n = 10'000'000;

	// range  [-1, 1]
	vector<Vector3> points = createData(n);


	double gridSize = 512;
	Vector3 min = { -1.0, -1.0, -1.0 };
	Vector3 max = { 1.0, 1.0, 1.0 };
	Vector3 size = max - min;

	

	//for (int i = 0; i < 10; i++) {
	//	countWithVector(gridSize, points, min, max);
	//}

	{
		double tStart = now();
		countWithVector(gridSize, points, min, max);
		printElapsedTime("vector", tStart);
	}
	
	{
		double tStart = now();
		countWithUnorderedMap(gridSize, points, min, max);
		printElapsedTime("unordered_map", tStart);
	}

	{
		double tStart = now();
		countWithTreeStructure(gridSize, points, min, max);
		printElapsedTime("tree structure", tStart);
	}

	//{
	//	double tStart = now();
	//	countWithCustomMap(gridSize, points, min, max);
	//	printElapsedTime("custom map", tStart);
	//}


	

}


void benchmarkRead() {
	string path = "C:/dev/pointclouds/morro_bay.las";
	size_t filesize = fs::file_size(path);
	filesize = filesize - filesize % (1024 * 1024);

	//filesize = 128;

	auto tStart = now();

	FILE* f = fopen(path.c_str(), "rb");
	uint32_t number = 0;
	int64_t count = 0;
	//void* buffer = &number;

	int64_t iterations = 0;
	int stepsize = 512 * 1024;
	//int stepsize = 128;
	void* buffer = malloc(stepsize);
	uint8_t* u8 = reinterpret_cast<uint8_t*>(buffer);
	for (size_t i = 0; i < filesize; i += stepsize) {
		int numRead = fread(buffer, 1, stepsize, f);

		for (int j = 0; j < stepsize; j += 4) {
			memcpy(&number, u8 + j, 4);
			count += number;
			iterations++;
		}

	}

	fclose(f);
	free(buffer);

	auto duration = now() - tStart;
	printElapsedTime("FILE", tStart);

	cout << "stepsize: " << stepsize << endl;
	cout << "iterations: " << iterations << endl;
	cout << "count: " << count << endl;


	
}







































namespace benchmark{

	uint8_t* createTestData(int64_t n, int64_t numAttributes) {

		int64_t bytesPerPoint = numAttributes * 4;
		void* buffer = malloc(n * bytesPerPoint);
		uint8_t* data = reinterpret_cast<uint8_t*>(buffer);

		for (int64_t i = 0; i < n; i++) {

			int64_t pointOffset = i * bytesPerPoint;

			for (int64_t j = 0; j < numAttributes; j++) {
				int64_t attributeOffset = pointOffset + j * 4;

				int32_t value = j;

				memcpy(data + attributeOffset, &value, 4);
			}

		}

		return data;
	}


	void method1(int64_t n, int64_t numAttributes, uint8_t* data) {

		cout << "== method 1 ==" << endl;


		vector<int64_t> sums(numAttributes, 0);
		vector<std::function<void(int64_t)>> attributeHandlers;

		auto tStart = now();

		for (int i = 0; i < numAttributes; i++) {
			auto& sum = sums[i];
			auto handleAttribute = [data, &sum](int64_t byteOffset) {
				int value = reinterpret_cast<int32_t*>(data + byteOffset)[0];

				sum += value;
			};

			attributeHandlers.push_back(handleAttribute);
		}

		int64_t offset = 0;
		for (int64_t i = 0; i < n; i++) {

			for (auto& handler : attributeHandlers) {
				handler(offset);

				offset += 4;
			}
		}

		auto duration = now() - tStart;
		cout << "duration: " << duration << "s" << endl;

		for (int i = 0; i < numAttributes; i++) {
			auto sum = sums[i];
			double average = double(sum) / double(n);

			cout << average << ", ";
		}

		cout << endl;
	}

	void method2(int64_t n, int64_t numAttributes, uint8_t* data) {

		cout << "== method 2 ==" << endl;


		vector<int64_t> sums(numAttributes, 0);
		vector<std::function<void(int64_t)>> attributeHandlers;

		for (int i = 0; i < numAttributes; i++) {
			auto& sum = sums[i];
			auto handleAttribute = [data, &sum](int64_t byteOffset) {
				int value = reinterpret_cast<int32_t*>(data + byteOffset)[0];

				sum += value;
			};

			attributeHandlers.push_back(handleAttribute);
		}

		auto& handler0 = attributeHandlers[0];
		auto& handler1 = attributeHandlers[1];
		auto& handler2 = attributeHandlers[2];
		auto& handler3 = attributeHandlers[3];
		auto& handler4 = attributeHandlers[4];
		auto& handler5 = attributeHandlers[5];
		auto& handler6 = attributeHandlers[6];
		auto& handler7 = attributeHandlers[7];
		auto& handler8 = attributeHandlers[8];
		auto& handler9 = attributeHandlers[9];

		auto tStart = now();

		int64_t offset = 0;
		for (int64_t i = 0; i < n; i++) {
			handler0(offset); offset += 4;
			handler1(offset); offset += 4;
			handler2(offset); offset += 4;
			handler3(offset); offset += 4;
			handler4(offset); offset += 4;
			handler5(offset); offset += 4;
			handler6(offset); offset += 4;
			handler7(offset); offset += 4;
			handler8(offset); offset += 4;
			handler9(offset); offset += 4;
		}

		auto duration = now() - tStart;
		cout << "duration: " << duration << "s" << endl;

		for (int i = 0; i < numAttributes; i++) {
			auto sum = sums[i];
			double average = double(sum) / double(n);

			cout << average << ", ";
		}
		cout << endl;
	}

	void run() {

		int64_t n = 100'000'000;
		int64_t numAttributes = 10;
		int64_t bytesPerPoint = numAttributes * 4;
		uint8_t* data = createTestData(n, numAttributes);

		method1(n, numAttributes, data);
		method2(n, numAttributes, data);
		method1(n, numAttributes, data);
		method2(n, numAttributes, data);
		method1(n, numAttributes, data);
		method2(n, numAttributes, data);

		//cout << "average: " << average << endl;

	}

}


#include "laszip/laszip_api.h"

namespace benchmark_poisson {

	struct Point {
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;

		uint8_t r = 0;
		uint8_t g = 0;
		uint8_t b = 0;
	};

	struct PointCloud {
		Vector3 min;
		Vector3 max;
		vector<Point> points;
	};

	PointCloud readFile(string path) {
		laszip_POINTER laszip_reader;
		laszip_header* header;
		laszip_point* laszip_point;
		{


			laszip_create(&laszip_reader);

			laszip_BOOL request_reader = 1;
			laszip_request_compatibility_mode(laszip_reader, request_reader);

			laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
			laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

			laszip_get_header_pointer(laszip_reader, &header);

			long long npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

			laszip_get_point_pointer(laszip_reader, &laszip_point);

			//laszip_seek_point(laszip_reader, task->firstPoint);
		}


		int64_t numPoints = std::max(header->extended_number_of_point_records, uint64_t(header->number_of_point_records));
		vector<Point> points;
		points.reserve(numPoints);

		double coordinates[3];

		for (int i = 0; i < numPoints; i++) {
			laszip_read_point(laszip_reader);
			laszip_get_coordinates(laszip_reader, coordinates);

			Point point;
			point.x = coordinates[0];
			point.y = coordinates[1];
			point.z = coordinates[2];

			point.r = laszip_point->rgb[0] > 255 ? laszip_point->rgb[0] / 256 : laszip_point->rgb[0];
			point.g = laszip_point->rgb[1] > 255 ? laszip_point->rgb[1] / 256 : laszip_point->rgb[1];
			point.b = laszip_point->rgb[2] > 255 ? laszip_point->rgb[2] / 256 : laszip_point->rgb[2];

			points.push_back(point);
		}

		laszip_close_reader(laszip_reader);
		laszip_destroy(laszip_reader);

		PointCloud pc;
		pc.points = points;
		pc.min = { header->min_x, header->min_y, header->min_z };
		pc.max = { header->max_x, header->max_y, header->max_z };

		return pc;
	}

	void save(vector<Point>& points, string path) {

		string text;

		for (Point point : points) {
			string line = to_string(point.x) + ", " + to_string(point.y) + ", " + to_string(point.z) + ", ";
			line += to_string(int(point.r)) + ", " + to_string(int(point.g)) + ", " + to_string(int(point.b));

			text += line + "\n";
		}

		writeFile(path, text);

	}

	void run() {

		string path = "D:/uni/octree_generation/benchmark/eclepens.las";
		//string path = "D:/uni/octree_generation/benchmark/heidentor.laz";
		//string path = "D:/dev/pointclouds/mschuetz/lion.las";

		auto pointcloud = readFile(path);
		auto center = (pointcloud.min + pointcloud.max) / 2.0;
		auto& points = pointcloud.points;

		auto tStart = now();

		auto parallel = std::execution::par_unseq;
		std::sort(parallel, points.begin(), points.end(), [center](Point a, Point b) -> bool {

			auto ax = a.x - center.x;
			auto ay = a.y - center.y;
			auto az = a.z - center.z;
			auto add = ax * ax + ay * ay + az * az;

			auto bx = b.x - center.x;
			auto by = b.y - center.y;
			auto bz = b.z - center.z;
			auto bdd = bx * bx + by * by + bz * bz;

			// sort by distance to center
			return add < bdd;
		});
		
		printElapsedTime("sort", tStart);

		double spacing = 2.0;
		double squaredSpacing = spacing * spacing;
		vector<Point> accepted;
		//accepted.reserve(1'000'000);

		auto squaredDistance = [](Point& a, Point& b) {
			double dx = a.x - b.x;
			double dy = a.y - b.y;
			double dz = a.z - b.z;

			double dd = dx * dx + dy * dy + dz * dz;

			return dd;
		};

		int64_t numDistanceChecks = 0;
		auto checkAccept = [&accepted, spacing, squaredSpacing, &squaredDistance, center, &numDistanceChecks](Point candidate) {

			auto cx = candidate.x - center.x;
			auto cy = candidate.y - center.y;
			auto cz = candidate.z - center.z;
			auto cdd = cx * cx + cy * cy + cz * cz;
			auto cd = sqrt(cdd);
			auto limit = (cd - spacing);
			auto limitSquared = limit * limit;

		
			int j = 0;
			for (int i = accepted.size() - 1; i >= 0; i--) {

				numDistanceChecks++;

				auto& point = accepted[i];

				// check distance to center
				auto px = point.x - center.x;
				auto py = point.y - center.y;
				auto pz = point.z - center.z;
				auto pdd = px * px + py * py + pz * pz;

				// stop when differences to center between candidate and accepted exceeds the spacing
				// any other previously accepted point will be even closer to the center.
				if (pdd < limitSquared) {
					return true;
				}


				double dd = squaredDistance(point, candidate);

				if (dd < squaredSpacing) {
					return false;
				}

				j++;

				// also put a limit at x distance checks
				if (j > 10'000) {
					return true;
				}
			}

			return true;

		};

		auto tStartChecks = now();

		for (Point point : points) {

			bool isAccepted = checkAccept(point);

			if (isAccepted) {
				accepted.push_back(point);
			} 
		}

		printElapsedTime("checks", tStartChecks);

		save(accepted, "D:/temp/poisson.csv");


		cout << "#points: " << points.size() << endl;
		cout << "#accepted: " << accepted.size() << endl;
		cout << "#distanceChecks: " << numDistanceChecks << endl;
		cout << "checks/point: " << (numDistanceChecks / points.size()) << endl;





	}

}












