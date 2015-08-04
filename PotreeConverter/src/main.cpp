
#include <chrono>
#include <vector>
#include <map>
#include <iostream>
#include <math.h>
#include <string>
#include <fstream>
#include <exception>

#include "Vector3.h"
#include "AABB.h"
#include "Point.h"
#include "stuff.h"
#include "PotreeWriter.hpp"

#include <boost/filesystem.hpp>

using std::ifstream;
using std::ofstream;
using std::ios;
using std::string;
using std::min;
using std::max;
using std::ostream;
using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::vector;
using std::binary_function;
using std::map;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using boost::filesystem::path;
using std::exception;
using Potree::PotreeWriter;


map<string, std::chrono::time_point<std::chrono::steady_clock> > times;

void startTime(string key){
	times[key] = high_resolution_clock::now();
}

void stopTime(string key){
	auto start = times[key];
	auto end = high_resolution_clock::now();
	long long duration = duration_cast<milliseconds>(end - start).count();
	float seconds = duration / 1000.0f;
	cout << key << ": " << seconds << "s" << endl;
}

int main(int argc, char **argv){

	
	//string fIn = "D:\\temp\\test\\lion.las";
	//string fIn = "D:\\temp\\perf\\ring.las";
	//string fIn = "D:\\temp\\perf\\ripple.las";
	string fIn = "C:\\temp\\test\\dechen_cave_upscaled.las";
	string fOut = "C:\\temp\\perf\\out.las";


	

	

	// -----------------------
	// READ
	// -----------------------
	startTime("read");
	std::ifstream ifs;
	ifs.open(fIn, std::ios::in | std::ios::binary);
	liblas::ReaderFactory f;
	liblas::Reader reader = f.CreateWithStream(ifs);
	liblas::Header const& header = reader.GetHeader();

	std::cout << "Compressed: " << (header.Compressed() == true) ? "true":"false";
	std::cout << "Signature: " << header.GetFileSignature() << '\n';
	std::cout << "Points count: " << header.GetPointRecordsCount() << '\n';

	double minX = header.GetMinX();
	double minY = header.GetMinY();
	double minZ = header.GetMinZ();
	double maxX = header.GetMaxX();
	double maxY = header.GetMaxY();
	double maxZ = header.GetMaxZ();
	double sizeX = maxX - minX;
	double sizeY = maxY - minY;
	double sizeZ = maxZ - minZ;
	double scaleX = header.GetScaleX();
	double scaleY = header.GetScaleY();
	double scaleZ = header.GetScaleZ();

	PotreeWriter *writer = new PotreeWriter();

	vector<Point> points;
	int i = 0;
	while (reader.ReadNextPoint()){
		liblas::Point const& p = reader.GetPoint();
		liblas::Color color = p.GetColor();

		unsigned char r = color.GetRed() / 256;
		unsigned char g = color.GetGreen() / 256;
		unsigned char b = color.GetBlue() / 256;

		writer->add(Point(p.GetX(), p.GetY(), p.GetZ(), r, g, b));

		if((writer->numPoints % 100000) == 0){
			cout << writer->numPoints << " written" << endl;
		}

		i++;
	} 

	stopTime("read");

	startTime("flush");
	writer->flush();
	stopTime("flush");
}

//
//
//#include <unordered_map>
//#include <random>
//#include <chrono>
//#include <iostream>
//#include <math.h>
//#include <algorithm>
//#include <string>
//
//using std::cout;
//using std::endl;
//using std::chrono::high_resolution_clock;
//using std::chrono::milliseconds;
//using std::chrono::duration_cast;
//using std::unordered_map;
//
//int num_elements = 5'000'000;
//
//void findThenInsert(){
//	cout << endl << "find and emplace" << endl;
//
//	auto start = high_resolution_clock::now();
//
//	std::mt19937 gen(123);
//	std::uniform_real_distribution<> dis(0, 128);
//
//	unordered_map<int, int> grid;
//	int count = 0;
//	for(int i = 0; i < num_elements; i++){
//		float x = dis(gen);
//		float y = dis(gen);
//		float z = (cos(x*0.1) * sin(x*0.1) + 1.0) * 64.0;
//
//		int index = int(x) + int(y) * 128 + int(z) * 128 * 128;
//		auto it = grid.find(index);
//		if(it == grid.end()){
//			grid.emplace(index, count);
//			count++;
//		}
//	}
//
//	cout << "elements: " << count << endl;
//	cout << "load factor: " << grid.load_factor() << endl;
//
//	auto end = high_resolution_clock::now();
//	long long duration = duration_cast<milliseconds>(end - start).count();
//	float seconds = duration / 1000.0f;
//	cout << seconds << "s" << endl;
//}
//
//void insertThenCheckForSuccess(){
//	cout << endl << "emplace and check success" << endl;
//
//	auto start = high_resolution_clock::now();
//
//	std::mt19937 gen(123);
//	std::uniform_real_distribution<> dis(0, 128);
//
//	unordered_map<int, int> grid;
//	int count = 0;
//	for(int i = 0; i < num_elements; i++){
//		float x = dis(gen);
//		float y = dis(gen);
//		float z = (cos(x*0.1) * sin(x*0.1) + 1.0) * 64.0;
//
//		int index = int(x) + int(y) * 128 + int(z) * 128 * 128;
//		auto it = grid.emplace(index, count);
//		if(it.second){
//			count++;
//		}
//	}
//
//	cout << "elements: " << count << endl;
//	cout << "load factor: " << grid.load_factor() << endl;
//
//	auto end = high_resolution_clock::now();
//	long long duration = duration_cast<milliseconds>(end - start).count();
//	float seconds = duration / 1000.0f;
//	cout << seconds << "s" << endl;
//}
//
//void test3(){
//	cout << endl << "test3" << endl;
//
//	auto start = high_resolution_clock::now();
//
//	std::mt19937 gen(123);
//	std::uniform_real_distribution<> dis(0, 128);
//
//	unordered_map<int, int> grid;
//	int count = 0;
//	for(int i = 0; i < num_elements; i++){
//		float x = dis(gen);
//		float y = dis(gen);
//		float z = (cos(x*0.1) * sin(x*0.1) + 1.0) * 64.0;
//
//		int index = int(x) + int(y) * 128 + int(z) * 128 * 128;
//		auto it = grid.find(index);
//		if(it == grid.end()){
//			grid.insert({index, count});
//			count++;
//		}
//	}
//
//	cout << "elements: " << count << endl;
//	cout << "load factor: " << grid.load_factor() << endl;
//
//	auto end = high_resolution_clock::now();
//	long long duration = duration_cast<milliseconds>(end - start).count();
//	float seconds = duration / 1000.0f;
//	cout << seconds << "s" << endl;
//}
//
//void test4(){
//	cout << endl << "emplace and check success" << endl;
//
//	auto start = high_resolution_clock::now();
//
//	std::mt19937 gen(123);
//	std::uniform_real_distribution<> dis(0, 128);
//
//	unordered_map<int, int> grid;
//	int count = 0;
//	for(int i = 0; i < num_elements; i++){
//		float x = dis(gen);
//		float y = dis(gen);
//		float z = (cos(x*0.1) * sin(x*0.1) + 1.0) * 64.0;
//
//		int index = int(x) + int(y) * 128 + int(z) * 128 * 128;
//		auto it = grid.insert({index, count});
//		if(it.second){
//			count++;
//		}
//	}
//
//	cout << "elements: " << count << endl;
//	cout << "load factor: " << grid.load_factor() << endl;
//
//	auto end = high_resolution_clock::now();
//	long long duration = duration_cast<milliseconds>(end - start).count();
//	float seconds = duration / 1000.0f;
//	cout << seconds << "s" << endl;
//}
//
//int main(){
//
//	findThenInsert();
//	insertThenCheckForSuccess();
//	test3();
//	test4();
//	
//}