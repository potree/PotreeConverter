

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "PTXPointReader.h"
#include "PotreeException.h"
#include "PotreeWriter.h"
#include "LASPointWriter.hpp"
#include "PlyPointReader.h"
#include "XYZPointReader.hpp"

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

PointReader *PotreeConverter::createPointReader(string path){
	PointReader *reader = NULL;
	if(boost::iends_with(path, ".las") || boost::iends_with(path, ".laz")){
		reader = new LASPointReader(path);
	}else if(boost::iends_with(path, ".ptx")){
		reader = new PTXPointReader(path);
	}else if(boost::iends_with(path, ".ply")){
		reader = new PlyPointReader(path);
	}else if(boost::iends_with(path, ".xyz") || boost::iends_with(path, ".txt")){
		reader = new XYZPointReader(path, format, colorRange, intensityRange);
	}else if(boost::iends_with(path, ".pts")){
		vector<double> intensityRange;

		if(this->intensityRange.size() == 0){
				intensityRange.push_back(-2048);
				intensityRange.push_back(+2047);
		}

		reader = new XYZPointReader(path, format, colorRange, intensityRange);
 	}

	return reader;
}

PotreeConverter::PotreeConverter(Options options){

	// if sources contains directories, use files inside the directory instead
	vector<string> sourceFiles;
	for(int i = 0; i < options.sources.size(); i++){
		string source = options.sources[i];
		path pSource(source);
		if(boost::filesystem::is_directory(pSource)){
			boost::filesystem::directory_iterator it(pSource);
			for(;it != boost::filesystem::directory_iterator(); it++){
				path pDirectoryEntry = it->path();
				if(boost::filesystem::is_regular_file(pDirectoryEntry)){
					string filepath = pDirectoryEntry.string();
					if(boost::iends_with(filepath, ".las") 
						|| boost::iends_with(filepath, ".laz") 
						|| boost::iends_with(filepath, ".xyz")
						|| boost::iends_with(filepath, ".pts")
						|| boost::iends_with(filepath, ".ptx")
						|| boost::iends_with(filepath, ".ply")){
						sourceFiles.push_back(filepath);
					}
				}
			}
		}else if(boost::filesystem::is_regular_file(pSource)){
			sourceFiles.push_back(source);
		}
	}
	

	this->sources = sourceFiles;
	this->workDir = options.workDir;
	this->spacing = options.spacing;
	this->maxDepth = options.maxDepth;
	this->format = options.format;
	this->colorRange = options.colorRange;
	this->intensityRange = options.intensityRange;
	this->scale = options.scale;
	this->outputFormat = options.outFormat;
	this->outputAttributes = options.outputAttributes;
	this->diagonalFraction = options.diagonalFraction;

	boost::filesystem::path dataDir(workDir + "/data");
	boost::filesystem::path tempDir(workDir + "/temp");
	fs::remove_all(dataDir);
	fs::remove_all(tempDir);
	boost::filesystem::create_directories(dataDir);
	boost::filesystem::create_directories(tempDir);

	cloudjs.octreeDir = "data";
	cloudjs.spacing = spacing;
	cloudjs.outputFormat = OutputFormat::LAS;
}


void PotreeConverter::convert(){
	long long pointsProcessed = 0;
	auto start = high_resolution_clock::now();

	// convert XYZ sources to las sources
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		if(boost::iends_with(source, ".txt") || boost::iends_with(source, ".xyz") || boost::iends_with(source, ".pts") || boost::iends_with(source, ".ptx")){
			boost::filesystem::path lasDir(workDir + "/las");
			boost::filesystem::create_directories(lasDir);
			string dest = workDir + "/las/" + fs::path(source).stem().string() + ".las";

			PointReader *reader = createPointReader(source);
			LASPointWriter *writer = new LASPointWriter(dest, aabb, 0.001);
			AABB aabb;

			while(reader->readNextPoint()){
				Point p = reader->getPoint();
				writer->write(p);

				Vector3<double> pos = p.position();
				aabb.update(pos);
				pointsProcessed++;

				if (0 == pointsProcessed  % 1000000) {
					auto end = high_resolution_clock::now();
					long long duration = duration_cast<milliseconds>(end - start).count();
					float seconds = duration / 1000.0f;
					stringstream ssMessage;
					ssMessage.imbue(std::locale(""));
					ssMessage << "CONVERT-LAS: ";
					ssMessage << pointsProcessed << " points processed; ";
					ssMessage << seconds << " seconds passed";

					cout << ssMessage.str() << endl;
				}
			}
			writer->header->SetMax(aabb.max.x, aabb.max.y, aabb.max.z);
			writer->header->SetMin(aabb.min.x, aabb.min.y, aabb.min.z);
			writer->writer->SetHeader(*writer->header);
			writer->writer->WriteHeader();

			reader->close();
			writer->close();

			delete reader;
			delete writer;

			sources[i] = dest;
		}
		
	}
	cout << endl;


	// calculate AABB and total number of points
	AABB aabb;
	long long numPoints = 0;
	for(string source : sources){

		PointReader *reader = createPointReader(source);
		AABB lAABB = reader->getAABB();
		 

		aabb.update(lAABB.min);
		aabb.update(lAABB.max);

		numPoints += reader->numPoints();

		reader->close();
		delete reader;

	}

	if(scale == 0){
		if(aabb.size.length() > 1000*1000){
			scale = 0.1;
		}else if(aabb.size.length() > 1000){
			scale = 0.01;
		}else if(aabb.size.length() > 1){
			scale = 0.001;
		}else{
			scale = 0.0001;
		}
	}


	if (diagonalFraction != 0) {
		spacing = (float)(aabb.size.length() / diagonalFraction);
		cout << "spacing calculated from diagonal: " << spacing << endl;
	}
	cout << "Last level will have spacing:     " << spacing / pow(2, maxDepth - 1) << endl;
	cout << endl;

	cout << "AABB: " << endl << aabb << endl;

	aabb.makeCubic();

	cout << "cubic AABB: " << endl << aabb << endl;

	cloudjs.boundingBox = aabb;

	start = high_resolution_clock::now();

	PotreeWriter writer(this->workDir, aabb, spacing, maxDepth, scale, outputFormat, outputAttributes);
	//PotreeWriterLBL writer(this->workDir, aabb, spacing, maxDepth, outputFormat);

	pointsProcessed = 0;
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];
		cout << "reading " << source << endl;

		PointReader *reader = createPointReader(source);
		while(reader->readNextPoint()){
			pointsProcessed++;
			//if((pointsProcessed%50) != 0){
			//	continue;
			//}

			Point p = reader->getPoint();
			writer.add(p);

			//if((pointsProcessed % (1000*1000)) == 0){
			//	//cout << (pointsProcessed / (1000*1000)) << "m points processed" << endl;
			//	cout << "flushing" << endl;
			//	writer.flush();
			//}

			if((pointsProcessed % (1000*1000)) == 0){
				writer.flush();

				auto end = high_resolution_clock::now();
				long long duration = duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1000.0f;

				stringstream ssMessage;

				ssMessage.imbue(std::locale(""));
				ssMessage << "INDEXING: ";
				ssMessage << pointsProcessed << " points processed; ";
				ssMessage << writer.numAccepted << " points written; ";
				ssMessage << seconds << " seconds passed";

				cout << ssMessage.str() << endl;

				//cout << (pointsProcessed / (1000*1000)) << "m points processed" << endl;
				//cout << "duration: " << (duration / 1000.0f) << "s" << endl;
			}
		}
		writer.flush();
		reader->close();
		delete reader;
	}
	
	cout << "closing writer" << endl;
	writer.close();

	float percent = (float)writer.numAccepted / (float)pointsProcessed;
	percent = percent * 100;

	auto end = high_resolution_clock::now();
	long long duration = duration_cast<milliseconds>(end-start).count();

	
	cout << endl;
	cout << "conversion finished" << endl;
	cout << pointsProcessed << " points were processed and " << writer.numAccepted << " points ( " << percent << "% ) were written to the output. " << endl;

	cout << "duration: " << (duration / 1000.0f) << "s" << endl;
}
