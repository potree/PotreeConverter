
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


int main(int argc, char **argv){
	cout << "start" << endl;

	string path = "D:/temp/test";
	PotreeWriter *writer = new PotreeWriter(path);

	for(int x = -10; x <= 10; x++){
		for(int y = -10; y <= 10; y++){
			double z = pow(x * 0.1, 2.0) * pow(y * 0.1, 2.0);

			writer->add(Point(x, y, z, 100, 100, 100));

		}
	}

	writer->flush();
	delete writer;
	

	cout << "end" << endl;
}
