
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
using Potree::PotreeConverter;

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid;

void printUsage(po::options_description &desc){
	cout << "usage: PotreeConverter [OPTIONS] SOURCE" << endl;
	cout << desc << endl;
}

struct Arguments{
	bool help = false;
	vector<string> source;
	string outdir;
	float spacing;
	int levels;
	string format;
	string outFormatString;
	double scale;
	int diagonalFraction;
	Potree::OutputFormat outFormat;
	vector<double> colorRange;
	vector<double> intensityRange;
	vector<string> outputAttributes;
	bool generatePage;
	string aabbValuesString;
	vector<double> aabbValues;
	string pageName = "";
};

Arguments parseArguments(int argc, char **argv){
	Arguments a;

	po::options_description desc("Options"); 
	desc.add_options() 
		("help,h", "prints usage")
		("generate-page,p", po::value<string>(&a.pageName), "Generates a ready to use web page with the given name.")
		("outdir,o", po::value<string>(&a.outdir), "output directory") 
		("spacing,s", po::value<float>(&a.spacing), "Distance between points at root level. Distance halves each level.") 
		("spacing-by-diagonal-fraction,d", po::value<int>(&a.diagonalFraction), "Maximum number of points on the diagonal in the first level (sets spacing). spacing = diagonal / value")
		("levels,l", po::value<int>(&a.levels), "Number of levels that will be generated. 0: only root, 1: root and its children, ...")
		("input-format,f", po::value<string>(&a.format), "Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number")
		("color-range", po::value<std::vector<double> >()->multitoken(), "")
		("intensity-range", po::value<std::vector<double> >()->multitoken(), "")
		("output-format", po::value<string>(&a.outFormatString), "Output format can be BINARY, LAS or LAZ. Default is BINARY")
		("output-attributes,a", po::value<std::vector<std::string> >()->multitoken(), "can be any combination of RGB, INTENSITY and CLASSIFICATION. Default is RGB.")
		("scale", po::value<double>(&a.scale), "Scale of the X, Y, Z coordinate in LAS and LAZ files.")
		("aabb", po::value<string>(&a.aabbValuesString), "Bounding cube as \"minX minY minZ maxX maxY maxZ\". If not provided it is automatically computed")
		("source", po::value<std::vector<std::string> >(), "Source file. Can be LAS, LAZ, PTX or PLY");
	po::positional_options_description p; 
	p.add("source", -1); 

	po::variables_map vm; 
	po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm); 
	po::notify(vm);

	a.help = vm.count("help") || !vm.count("source");

	if(vm.count("source")){
		a.source = vm["source"].as<std::vector<std::string> >();
	}else{
		cerr << "source file parameter is missing" << endl;
		exit(1);
	}

	if(vm.count("color-range")){
		a.colorRange = vm["color-range"].as< vector<double> >();

		if(a.colorRange.size() > 2){
			cerr << "color-range only takes 0 - 2 arguments" << endl;
			exit(1);
		}
	}

	if(vm.count("intensity-range")){
		a.intensityRange = vm["intensity-range"].as< vector<double> >();

		if(a.intensityRange.size() > 2){
			cerr << "intensity-range only takes 0 - 2 arguments" << endl;
			exit(1);
		}
	}

	if(vm.count("output-attributes")){
		a.outputAttributes = vm["output-attributes"].as< vector<string> >();
	}else{
		a.outputAttributes.push_back("RGB");
	}


	if(vm.count("aabb")){
		char sep = ' '; 
		for(size_t p=0, q=0; p!= a.aabbValuesString.npos; p=q)
    		a.aabbValues.push_back(atof(a.aabbValuesString.substr(p+(p!=0), (q = a.aabbValuesString.find(sep, p+1))-p-(p!=0)).c_str())); 

		if(a.aabbValues.size() != 6){
			cerr << "AABB requires 6 arguments" << endl;
			exit(1);
		}
	}

	// set default parameters 
	path pSource(a.source[0]);
	a.outdir = vm.count("outdir") ? vm["outdir"].as<string>() : pSource.generic_string() + "_converted";
	if(!vm.count("spacing")) a.spacing = 0;
	a.generatePage = (!vm.count("generate-page")) ? false : true;
	if(!vm.count("spacing-by-diagonal-fraction")) a.diagonalFraction = 0;
	if(!vm.count("levels")) a.levels = -1;
	if(!vm.count("input-format")) a.format = "";
	if(!vm.count("scale")) a.scale = 0;
	if(!vm.count("output-format")) a.outFormatString = "BINARY";
	if(a.outFormatString == "BINARY"){
		a.outFormat = Potree::OutputFormat::BINARY;
	}else if(a.outFormatString == "LAS"){
		a.outFormat = Potree::OutputFormat::LAS;
	}else if(a.outFormatString == "LAZ"){
		a.outFormat = Potree::OutputFormat::LAZ;
	}
	if (a.diagonalFraction != 0) {
		a.spacing = 0;
	}else if(a.spacing == 0){
		a.diagonalFraction = 250;
	}

	return a;
}

void printArguments(Arguments &a){
	try{

		cout << "== params ==" << endl;
		int i = 0;
		for(const auto &s : a.source) {
			cout << "source[" << i << "]:         \t" << a.source[i] << endl;
			++i;
		}
		cout << "outdir:            \t" << a.outdir << endl;
		cout << "spacing:           \t" << a.spacing << endl;
		cout << "diagonal-fraction: \t" << a.diagonalFraction << endl;
		cout << "levels:            \t" << a.levels << endl;
		cout << "format:            \t" << a.format << endl;
		cout << "scale:             \t" << a.scale << endl;
		cout << "pageName:          \t" << a.pageName << endl;
		cout << "output-format:     \t" << a.outFormatString << endl;
		cout << endl;
	}catch(exception &e){
		cout << "ERROR: " << e.what() << endl;

		exit(1);
	}
}

int main(int argc, char **argv){
	cout.imbue(std::locale(""));
	
	Arguments a = parseArguments(argc, argv);
	printArguments(a);
	
	try{
		PotreeConverter pc(a.outdir, a.source);

		pc.spacing = a.spacing;
		pc.diagonalFraction = a.diagonalFraction;
		pc.maxDepth = a.levels;
		pc.format = a.format;
		pc.colorRange = a.colorRange;
		pc.intensityRange = a.intensityRange;
		pc.scale = a.scale;
		pc.outputFormat = a.outFormat;
		pc.outputAttributes = a.outputAttributes;
		pc.aabbValues = a.aabbValues;
		pc.pageName = a.pageName;

		pc.convert();
	}catch(exception &e){
		cout << "ERROR: " << e.what() << endl;
		return 1;
	}
	
	return 0;
}
