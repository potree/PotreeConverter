

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

#include "boost/program_options.hpp" 
#include <boost/filesystem.hpp>

namespace po = boost::program_options; 

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
using boost::filesystem::path;

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid;

void printUsage(po::options_description &desc){
	cout << "usage: PotreeConverter [OPTIONS] SOURCE" << endl;
	cout << desc << endl;
}

#include "XYZPointReader.h"

int main(int argc, char **argv){

	//string file = "C:/dev/pointclouds/testclouds/bunny.xyz";
	//string format = "xyzi";
	//float range = 1;
	//XYZPointReader reader(file, format, range);
	//reader.readNextPoint();
	//Point p = reader.getPoint();
	//cout << p.x << ", " << p.y << ", " << p.z << " | " <<int( p.r) << ", " << int(p.g) << ", " << int(p.b) << endl;


	string source;
	string outdir;
	float spacing;
	int levels;
	string format;
	float range;

	// read parameters from command line
	po::options_description desc("Options"); 
    desc.add_options() 
		("help,h", "prints usage")
		("outdir,o", po::value<string>(&outdir), "output directory") 
		("spacing,s", po::value<float>(&spacing), "Distance between points at root level. Distance halves each level.") 
		("levels,l", po::value<int>(&levels), "Number of levels that will be generated. 0: only root, 1: root and its children, ...")
		("input-format,f", po::value<string>(&format), "Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number")
		("range,r", po::value<float>(&range), "Range of rgb or intensity. ")
		("source", po::value<std::vector<std::string>>(), "Source file. Can be LAS, PLY or XYZ");
	po::positional_options_description p; 
	p.add("source", -1); 

	po::variables_map vm; 
	po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm); 
	po::notify(vm);

	if(vm.count("help") || !vm.count("source")){
		printUsage(desc);

		return 0;
	}

	if(vm.count("source")){
		std::vector<std::string> files = vm["source"].as<std::vector<std::string>>();
		source = files[0];
	}else{
		cout << "source file parameter is missing" << endl;
		return 1;
	}

	// set default parameters 
	path pSource(source);
	//outdir = vm.count("outdir") ? vm["outdir"].as<string>() : pSource.parent_path().generic_string() + "/potree_converted";
	outdir = vm.count("outdir") ? vm["outdir"].as<string>() : pSource.generic_string() + "_converted";
	if(!vm.count("spacing")) spacing = 1.0;
	if(!vm.count("levels")) levels = 3;
	if(!vm.count("input-format")) format = "xyzrgb";
	if(!vm.count("range")) range = 255;

	cout << "source: " << source << endl;
	cout << "outdir: " << outdir << endl;
	cout << "spacing: " << spacing << endl;
	cout << "levels: " << levels << endl;
	cout << "format: " << format << endl;
	cout << "range: " << range << endl;
	
	auto start = high_resolution_clock::now();
	
	PotreeConverter pc(source, outdir, spacing, levels, format, range);
	pc.convert();
	
	auto end = high_resolution_clock::now();
	milliseconds duration = duration_cast<milliseconds>(end-start);
	cout << "duration: " << (duration.count()/1000.0f) << "s" << endl;


	return 0;
}