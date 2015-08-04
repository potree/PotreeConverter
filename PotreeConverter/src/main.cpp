
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

	
	string fIn = "D:\\temp\\test\\lion.las";
	//string fIn = "D:\\temp\\perf\\ring.las";
	//string fIn = "D:\\temp\\perf\\ripple.las";
	//string fIn = "C:\\temp\\test\\dechen_cave_upscaled.las";
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
