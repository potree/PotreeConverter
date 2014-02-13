

#include <chrono>
#include <vector>
#include <map>
#include <iostream>
#include <math.h>
#include <string>
#include <fstream>

#include "Vector3.h"
#include "AABB.h"
#include "Point.h"
#include "GridIndex.h"
#include "SparseGrid.h"
#include "GridCell.h"
#include "PotreeConverter.h"

using std::ifstream;
using std::ofstream;
using std::ios;
using std::string;
using std::min;
using std::max;
using std::ostream;
using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::binary_function;
using std::map;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid;

//xyzrgb2bin C:/dev/pointclouds/map.archi.fr/Pompei_Ortho.xyz C:/dev/pointclouds/map.archi.fr/Pompei_test.bin 255
//PotreeConverter.exe C:/dev/pointclouds/map.archi.fr/Pompei_test.bin C:/dev/pointclouds/map.archi.fr/pompei_test 1.0 5

int main(int arg_c, char**argv){

	if(arg_c != 5){
		cout << "usage: " << endl;
		cout << "PotreeConverter <sourcePath> <targetPath> <pointDensity> <recursionDepth>" << endl;
		return 1;
	}

	string fData = argv[1];
	string workDir = argv[2];
	float minGap = atof(argv[3]);
	int maxDepth = atoi(argv[4]);

	auto start = high_resolution_clock::now();

	PotreeConverter pc(fData, workDir, minGap, maxDepth);
	pc.convert();

	auto end = high_resolution_clock::now();
	milliseconds duration = duration_cast<milliseconds>(end-start);
	cout << "duration: " << (duration.count()/1000.0f) << "s" << endl;

	cout << "finished" << endl;
}
