

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "PTXPointReader.h"
#include "PotreeException.h"
#include "PotreeWriter.h"
#include "LASPointWriter.hpp"
#include "BINPointWriter.hpp"
#include "BINPointReader.hpp"
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

namespace Potree{

PointReader *PotreeConverter::createPointReader(string path, PointAttributes pointAttributes){
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
 	}else if(boost::iends_with(path, ".bin")){
		reader = new BINPointReader(path, aabb, scale, pointAttributes);
	}

	return reader;
}

PotreeConverter::PotreeConverter(string workDir, vector<string> sources){
	this->workDir = workDir;
	this->sources = sources;
}

void PotreeConverter::prepare(){

	// if sources contains directories, use files inside the directory instead
	vector<string> sourceFiles;
	for (const auto &source : sources) {
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

	pointAttributes = PointAttributes();
	pointAttributes.add(PointAttribute::POSITION_CARTESIAN);
	for(const auto &attribute : outputAttributes){
		if(attribute == "RGB"){
			pointAttributes.add(PointAttribute::COLOR_PACKED);
		}else if(attribute == "INTENSITY"){
			pointAttributes.add(PointAttribute::INTENSITY);
		}else if(attribute == "CLASSIFICATION"){
			pointAttributes.add(PointAttribute::CLASSIFICATION);
		}else if(attribute == "NORMAL"){
			pointAttributes.add(PointAttribute::NORMAL_OCT16);
		}
	}
}

AABB PotreeConverter::calculateAABB(){
	AABB aabb;
	if(aabbValues.size() == 6){
		Vector3<double> userMin(aabbValues[0],aabbValues[1],aabbValues[2]);
		Vector3<double> userMax(aabbValues[3],aabbValues[4],aabbValues[5]);
		aabb = AABB(userMin, userMax);
	}else{
		for(string source : sources){

			PointReader *reader = createPointReader(source, pointAttributes);
			
			AABB lAABB = reader->getAABB();
			aabb.update(lAABB.min);
			aabb.update(lAABB.max);

			reader->close();
			delete reader;
		}
	}

	return aabb;
}

void PotreeConverter::convert(){
	auto start = high_resolution_clock::now();

	prepare();

	long long pointsProcessed = 0;

	AABB aabb = calculateAABB();
	cout << "AABB: " << endl << aabb << endl;
	aabb.makeCubic();
	cout << "cubic AABB: " << endl << aabb << endl;

	if (diagonalFraction != 0) {
		spacing = (float)(aabb.size.length() / diagonalFraction);
		cout << "spacing calculated from diagonal: " << spacing << endl;
	}

	PotreeWriter writer(this->workDir, aabb, spacing, maxDepth, scale, outputFormat, pointAttributes);
	if(pageName.size() > 0){
		writer.generatePage(pageName);
	}

	for (const auto &source : sources) {
		cout << "READING:  " << source << endl;

		PointReader *reader = createPointReader(source, pointAttributes);
		while(reader->readNextPoint()){
			pointsProcessed++;

			Point p = reader->getPoint();
			writer.add(p);

			if((pointsProcessed % (1'000'000)) == 0){
				writer.processStore();
				writer.waitUntilProcessed();

				auto end = high_resolution_clock::now();
				long long duration = duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1'000.0f;

				stringstream ssMessage;

				ssMessage.imbue(std::locale(""));
				ssMessage << "INDEXING: ";
				ssMessage << pointsProcessed << " points processed; ";
				ssMessage << writer.numAccepted << " points written; ";
				ssMessage << seconds << " seconds passed";

				cout << ssMessage.str() << endl;
			}
			if((pointsProcessed % (10'000'000)) == 0){
				cout << "FLUSHING: ";
			
				auto start = high_resolution_clock::now();
			
				writer.flush();
			
				auto end = high_resolution_clock::now();
				long long duration = duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1'000.0f;
			
				cout << seconds << "s" << endl;
			}

			//if(pointsProcessed >= 10'000'000){
			//	break;
			//}
		}
		reader->close();
		delete reader;
	}
	
	cout << "closing writer" << endl;
	writer.flush();
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

}
