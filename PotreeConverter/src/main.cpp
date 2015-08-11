
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
#include "stuff.h"

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

#define MAX_FLOAT std::numeric_limits<float>::max()

class SparseGrid;

void printUsage(po::options_description &desc){
	cout << "usage: PotreeConverter [OPTIONS] SOURCE" << endl;
	cout << desc << endl;
}

int main(int argc, char **argv){
	vector<string> source;
	string outdir;
	float spacing;
	int levels;
	string format;
	string outFormatString;
	double scale;
	int diagonalFraction;
	OutputFormat outFormat;
	vector<double> colorRange;
	vector<double> intensityRange;
	vector<string> outputAttributes;
	bool generatePage;
	string aabbValuesString;
	vector<double> aabbValues;

	cout.imbue(std::locale(""));

	try{
		// read parameters from command line
		po::options_description desc("Options"); 
		desc.add_options() 
			("help,h", "prints usage")
			("generate-page,p", "Generates a ready to use web page along with the model.")
			("outdir,o", po::value<string>(&outdir), "output directory") 
			("spacing,s", po::value<float>(&spacing), "Distance between points at root level. Distance halves each level.") 
			("spacing-by-diagonal-fraction,d", po::value<int>(&diagonalFraction), "Maximum number of points on the diagonal in the first level (sets spacing). spacing = diagonal / value")
			("levels,l", po::value<int>(&levels), "Number of levels that will be generated. 0: only root, 1: root and its children, ...")
			("input-format,f", po::value<string>(&format), "Input format. xyz: cartesian coordinates as floats, rgb: colors as numbers, i: intensity as number")
			("color-range", po::value<std::vector<double> >()->multitoken(), "")
			("intensity-range", po::value<std::vector<double> >()->multitoken(), "")
			("output-format", po::value<string>(&outFormatString), "Output format can be BINARY, LAS or LAZ. Default is BINARY")
			("output-attributes,a", po::value<std::vector<std::string> >()->multitoken(), "can be any combination of RGB, INTENSITY and CLASSIFICATION. Default is RGB.")
			("scale", po::value<double>(&scale), "Scale of the X, Y, Z coordinate in LAS and LAZ files.")
			("aabb", po::value<string>(&aabbValuesString), "Bounding cube as \"minX minY minZ maxX maxY maxZ\". If not provided it is automatically computed")
			("source", po::value<std::vector<std::string> >(), "Source file. Can be LAS, LAZ, PTX or PLY");
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
			cerr << "source file parameter is missing" << endl;
			return 1;
		}

		if(vm.count("color-range")){
			colorRange = vm["color-range"].as< vector<double> >();

			if(colorRange.size() > 2){
				cerr << "color-range only takes 0 - 2 arguments" << endl;
				return 1;
			}
		}

		if(vm.count("intensity-range")){
			intensityRange = vm["intensity-range"].as< vector<double> >();

			if(intensityRange.size() > 2){
				cerr << "intensity-range only takes 0 - 2 arguments" << endl;
				return 1;
			}
		}

		if(vm.count("output-attributes")){
			outputAttributes = vm["output-attributes"].as< vector<string> >();
		}else{
			outputAttributes.push_back("RGB");
		}


		if(vm.count("aabb")){
			char sep = ' '; 
			for(size_t p=0, q=0; p!=aabbValuesString.npos; p=q)
    			aabbValues.push_back(atof(aabbValuesString.substr(p+(p!=0), (q=aabbValuesString.find(sep, p+1))-p-(p!=0)).c_str())); 

			if(aabbValues.size() != 6){
				cerr << "AABB requires 6 arguments" << endl;
				return 1;
			}
		}

		// set default parameters 
		path pSource(source[0]);
		outdir = vm.count("outdir") ? vm["outdir"].as<string>() : pSource.generic_string() + "_converted";
		if(!vm.count("spacing")) spacing = 0;
		generatePage = (!vm.count("generate-page")) ? false : true;
		if(!vm.count("spacing-by-diagonal-fraction")) diagonalFraction = 0;
		if(!vm.count("levels")) levels = -1;
		if(!vm.count("input-format")) format = "";
		if(!vm.count("scale")) scale = 0;
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
		int i = 0;
		for(const auto &s : source) {
			cout << "source[" << i << "]:         \t" << source[i] << endl;
			++i;
		}
		cout << "outdir:            \t" << outdir << endl;
		cout << "spacing:           \t" << spacing << endl;
		cout << "diagonal-fraction: \t" << diagonalFraction << endl;
		cout << "levels:            \t" << levels << endl;
		cout << "format:            \t" << format << endl;
		cout << "scale:             \t" << scale << endl;
		cout << "output-format:     \t" << outFormatString << endl;
		cout << endl;
	}catch(exception &e){
		cout << "ERROR: " << e.what() << endl;

		return 1;
	}
	
	auto start = high_resolution_clock::now();

	if(generatePage){
		//string pointCloudName = fs::path(source[0]).stem().string();
		string pointCloudName = fs::path(source[0]).filename().string();
		string pagedir = outdir;
		outdir += "/resources/pointclouds/" + pointCloudName;
		//string templateSourcePath = pagedir + "/examples/viewer_template.html";
		string templateSourcePath = "./resources/page_template/examples/viewer_template.html";
		string templateTargetPath = pagedir + "/examples/" + pointCloudName + ".html";

		//cout << "copy './resources/page_template' to '" << pagedir << "'" << endl;
		copyDir(fs::path("./resources/page_template"), fs::path(pagedir));
		//fs::rename(fs::path(templateSourcePath), fs::path(templateTargetPath));
		fs::remove(pagedir + "/examples/viewer_template.html");

		{ // change viewer template
			ifstream in( templateSourcePath );
			ofstream out( templateTargetPath );

			string line;
			while(getline(in, line)){
				if(line.find("<!-- INCLUDE SETTINGS HERE -->") != string::npos){
					out << "\t<script src=\"./" << pointCloudName << ".js\"></script>" << endl;
				}else if((outFormat == OutputFormat::LAS || outFormat == OutputFormat::LAZ) && 
					line.find("<!-- INCLUDE ADDITIONAL DEPENDENCIES HERE -->") != string::npos){

					out << "\t<script src=\"../libs/plasio/js/laslaz.js\"></script>" << endl;
					out << "\t<script src=\"../libs/plasio/vendor/bluebird.js\"></script>" << endl;
					out << "\t<script src=\"../build/js/laslaz.js\"></script>" << endl;
				}else{
					out << line << endl;
				}
				
			}

			in.close();
			out.close();
		}


		{ // write settings
			stringstream ssSettings;

			ssSettings << "var sceneProperties = {" << endl;
			ssSettings << "\tpath: \"" << "../resources/pointclouds/" << pointCloudName << "/cloud.js\"," << endl;
			ssSettings << "\tcameraPosition: null, 		// other options: cameraPosition: [10,10,10]," << endl;
			ssSettings << "\tcameraTarget: null, 		// other options: cameraTarget: [0,0,0]," << endl;
			ssSettings << "\tfov: 60, 					// field of view in degrees," << endl;
			ssSettings << "\tsizeType: \"Adaptive\",	// other options: \"Fixed\", \"Attenuated\"" << endl;
			ssSettings << "\tquality: null, 			// other options: \"Circles\", \"Interpolation\", \"Splats\"" << endl;
			ssSettings << "\tmaterial: \"RGB\", 		// other options: \"Height\", \"Intensity\", \"Classification\"" << endl;
			ssSettings << "\tpointLimit: 1,				// max number of points in millions" << endl;
			ssSettings << "\tpointSize: 1,				// " << endl;
			ssSettings << "\tnavigation: \"Orbit\",		// other options: \"Orbit\", \"Flight\"" << endl;
			ssSettings << "\tuseEDL: false,				" << endl;
			ssSettings << "};" << endl;

		
			ofstream fSettings;
			fSettings.open(pagedir + "/examples/" + pointCloudName + ".js", ios::out);
			fSettings << ssSettings.str();
			fSettings.close();
		}
	}
	
	try{
		PotreeConverter pc(source, outdir, spacing, diagonalFraction, levels, format, colorRange, intensityRange, scale, outFormat, outputAttributes, aabbValues);
		pc.convert();
	}catch(exception &e){
		cout << "ERROR: " << e.what() << endl;
		return 1;
	}

	
	
	auto end = high_resolution_clock::now();
	milliseconds duration = duration_cast<milliseconds>(end-start);


	return 0;
}
