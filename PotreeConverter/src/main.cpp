
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
#include "GridIndex.h"
#include "SparseGrid.h"
#include "GridCell.h"
#include "PotreeConverter.h"
#include "PotreeException.h"

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
using std::exception;

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid;

void printUsage(po::options_description &desc){
	cout << "usage: PotreeConverter [OPTIONS] SOURCE" << endl;
	cout << desc << endl;
}

#include "LASPointReader.h"
#include "LASPointWriter.hpp"
#include "BINPointWriter.hpp"

//int main(int argc, char **argv){
//	string path = "C:/dev/workspaces/potree/develop/resources/pointclouds/lion_takanawa/laz/r.laz";
//	string pathOut = "C:/temp/test.las";
//	LASPointReader *reader = new LASPointReader(path);
//	LASPointWriter *writer = new LASPointWriter(pathOut, reader->getAABB());
//
//	int i = 0;
//	while(reader->readNextPoint()){
//		Point p = reader->getPoint();
//		
//		if(i < 10){
//			cout << p.position() << endl;
//		}
//
//		writer->write(p);
//		i++;
//	}
//	writer->close(); 
//	reader->close();
//}

//#include "BINPointReader.hpp"
//#include "BINPointWriter.hpp"
//
//int main(int argc, char **argv){
//	string path = "C:/dev/pointclouds/converted/skatepark/data/r";
//	string pathOut = "C:/temp/skatepark";
//	BINPointReader *reader = new BINPointReader(path);
//	BINPointWriter *writer = new BINPointWriter(pathOut);
//
//	int i = 0;
//	while(reader->readNextPoint()){
//		Point p = reader->getPoint();
//		
//		if(i < 10){
//			cout << p.position() << endl;
//		}
//
//		writer->write(p);
//		i++;
//	}
//	writer->close();
//	reader->close();
//}

int main(int argc, char **argv){
	vector<string> source;
	string outdir;
	float spacing;
	int levels;
	string format;
	float range;
	string outFormatString;
	double scale;
	int diagonalFraction;
	double minSpacing;
	OutputFormat outFormat;

	cout.imbue(std::locale(""));

	try{
		// read parameters from command line
		po::options_description desc("Options"); 
		desc.add_options() 
			("help,h", "prints usage")
			("outdir,o", po::value<string>(&outdir), "output directory") 
			("spacing,s", po::value<float>(&spacing), "Distance between points at root level. Distance halves each level.") 
			("spacing-by-diagonal-fraction,d", po::value<int>(&diagonalFraction), "Maximum number of points on the diagonal in the first level (sets spacing). spacing = diagonal / value")
			("min-spacing,m", po::value<double>(&minSpacing), "Minimum allowed spacing at the lowest level (sets levels).")
			("levels,l", po::value<int>(&levels), "Number of levels that will be generated. 0: only root, 1: root and its children, ...")
			("input-format,f", po::value<string>(&format), "Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number")
			("range,r", po::value<float>(&range), "Range of rgb or intensity. ")
			("output-format", po::value<string>(&outFormatString), "Output format can be BINARY, LAS or LAZ. Default is BINARY")
			("scale", po::value<double>(&scale), "Scale of the X, Y, Z coordinate in LAS and LAZ files.")
			("source", po::value<std::vector<std::string> >(), "Source file. Can be LAS, LAZ or PLY");
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
			source = vm["source"].as<std::vector<std::string> >();
		}else{
			cout << "source file parameter is missing" << endl;
			return 1;
		}

		// set default parameters 
		path pSource(source[0]);
		outdir = vm.count("outdir") ? vm["outdir"].as<string>() : pSource.generic_string() + "_converted";
		if(!vm.count("spacing")) spacing = 0;
		if(!vm.count("spacing-by-diagonal-fraction")) diagonalFraction = 0;
		if(!vm.count("min-spacing")) minSpacing = 0.0;
		if(!vm.count("levels")) levels = 3;
		if(!vm.count("input-format")) format = "xyzrgb";
		if(!vm.count("range")) range = 255;
		if(!vm.count("scale")) scale = 0.01;
		if(!vm.count("output-format")) outFormatString = "BINARY";
		if(outFormatString == "BINARY"){
			outFormat = OutputFormat::BINARY;
		}else if(outFormatString == "LAS"){
			outFormat = OutputFormat::LAS;
		}else if(outFormatString == "LAZ"){
			outFormat = OutputFormat::LAZ;
		}
		if (diagonalFraction != 0) {
			spacing = 0;
		}else if(spacing == 0){
				diagonalFraction = 250;
		}

		cout << "== params ==" << endl;
		for(int i = 0; i < source.size(); i++){
			cout << "source[" << i << "]:         \t" << source[i] << endl;
		}
		cout << "outdir:            \t" << outdir << endl;
		cout << "spacing:           \t" << spacing << endl;
		cout << "diagonal-fraction: \t" << diagonalFraction << endl;
		cout << "levels:            \t" << levels << endl;
		cout << "format:            \t" << format << endl;
		cout << "range:             \t" << range << endl;
		cout << "scale:             \t" << scale << endl;
		cout << "output-format:     \t" << outFormatString << endl;
		cout << endl;
	}catch(exception &e){
		cout << "ERROR: " << e.what() << endl;

		return 1;
	}
	
	auto start = high_resolution_clock::now();
	
	try{
		PotreeConverter pc(source, outdir, spacing, diagonalFraction, levels, minSpacing, format, range, scale, outFormat);
		pc.convert();
	}catch(exception &e){
		cout << "ERROR: " << e.what() << endl;
		return 1;
	}
	
	auto end = high_resolution_clock::now();
	milliseconds duration = duration_cast<milliseconds>(end-start);
	//cout << "duration: " << (duration.count()/1000.0f) << "s" << endl;


	return 0;
}
