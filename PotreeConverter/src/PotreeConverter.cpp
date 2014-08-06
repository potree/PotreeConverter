

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "lasdefinitions.hpp"
#include "lasreader.hpp"
#include "laswriter.hpp"

#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "PotreeException.h"
#include "PotreeWriter.h"
#include "LASPointReader.h"
#include "LASPointWriter.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <math.h>
#include <fstream>

using std::stringstream;
using std::map;
using std::string;
using std::vector;
using std::find;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::fstream;
using boost::iends_with;
using boost::filesystem::is_directory;
using boost::filesystem::directory_iterator;
using boost::filesystem::is_regular_file;
using boost::filesystem::path;

struct Task{
	string source;
	string target;
	AABB aabb;

	Task(string source, string target, AABB aabb){
		this->source = source;
		this->target = target;
		this->aabb = aabb;
	}
};

PotreeConverter::PotreeConverter(vector<string> sources, string workDir, float spacing, int maxDepth, string format, float range, OutputFormat outFormat){

	// if sources contains directories, use files inside the directory instead
	vector<string> sourceFiles;
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];
		path pSource(source);
		if(boost::filesystem::is_directory(pSource)){
			boost::filesystem::directory_iterator it(pSource);
			for(;it != boost::filesystem::directory_iterator(); it++){
				path pDirectoryEntry = it->path();
				if(boost::filesystem::is_regular_file(pDirectoryEntry)){
					string filepath = pDirectoryEntry.string();
					if(boost::iends_with(filepath, ".las") || boost::iends_with(filepath, ".laz")){
						sourceFiles.push_back(filepath);
					}
				}
			}
		}else if(boost::filesystem::is_regular_file(pSource)){
			sourceFiles.push_back(source);
		}
	}
	

	this->sources = sourceFiles;
	this->workDir = workDir;
	this->spacing = spacing;
	this->maxDepth = maxDepth;
	this->format = format;
	this->range = range;
	this->outputFormat = outFormat;

	boost::filesystem::path dataDir(workDir + "/data");
	boost::filesystem::path tempDir(workDir + "/temp");
	boost::filesystem::create_directories(dataDir);
	boost::filesystem::create_directories(tempDir);

	cloudjs.octreeDir = "data";
	cloudjs.spacing = spacing;
	cloudjs.outputFormat = OutputFormat::LAS;
}


string PotreeConverter::getOutputExtension(){
	string outputExtension = "";
	if(outputFormat == OutputFormat::LAS){
		outputExtension = ".las";
	}else if(outputFormat == OutputFormat::LAZ){
		outputExtension = ".laz";
	}

	return outputExtension;
}

AABB calculateAABB(vector<string> sources){
	AABB aabb;

	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		LASPointReader reader(source);
		 const LASheader &header = reader.getHeader();

		Vector3<double> lmin = Vector3<double>(header.min_x, header.min_y, header.min_z);
		Vector3<double> lmax = Vector3<double>(header.max_x, header.max_y, header.max_z);

		aabb.update(lmin);
		aabb.update(lmax);

		reader.close();
	}

	return aabb;
}

void PotreeConverter::convert(){
	aabb = calculateAABB(sources);
	cout << "AABB: " << endl << aabb << endl;

	cloudjs.boundingBox = aabb;

	PotreeWriter writer(this->workDir, aabb, spacing, 5);

	long long pointsProcessed = 0;
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		LASPointReader reader(source);
		while(reader.readNextPoint()){
			pointsProcessed++;
			//if((pointsProcessed%50) != 0){
			//	continue;
			//}

			Point p = reader.getPoint();
			writer.add(p);

			if((pointsProcessed % (100*1000)) == 0){
				cout << (pointsProcessed / (100*1000)) << "m points processed" << endl;

				writer.flush();
			}

			//if(pointsProcessed >= (300*1000)){
			//	_CrtDumpMemoryLeaks();
			//	return;
			//}
		}
		reader.close();
	}
	
	writer.close();
}
