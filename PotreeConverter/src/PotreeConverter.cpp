

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "PotreeException.h"
#include "PotreeWriterDartThrowing.h"
#include "PotreeWriterRandom.hpp"
#include "LASPointReader.h"
#include "LASPointWriter.hpp"
#include "PlyPointReader.h"

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <math.h>
#include <fstream>
#include <iomanip>

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

PointReader *createPointReader(string path){
	PointReader *reader = NULL;
	if(boost::iends_with(path, ".las") || boost::iends_with(path, ".laz")){
		reader = new LASPointReader(path);
	}else if(boost::iends_with(path, ".ply")){
		reader = new PlyPointReader(path);
	}

	return reader;
}

PotreeConverter::PotreeConverter(vector<string> sources, string workDir, float spacing, int diagonalFraction, int maxDepth, string format, float range, double scale, OutputFormat outFormat, SelectionAlgorithm selectionAlgorithm){
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
					if(boost::iends_with(filepath, ".las") || boost::iends_with(filepath, ".laz") || boost::iends_with(filepath, ".ply")){
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
	this->scale = scale;
	this->outputFormat = outFormat;
	this->diagonalFraction = diagonalFraction;
	this->algorithm = selectionAlgorithm;

	boost::filesystem::path dataDir(workDir + "/data");
	boost::filesystem::path tempDir(workDir + "/temp");
	boost::filesystem::create_directories(dataDir);
	boost::filesystem::create_directories(tempDir);

	cloudjs.octreeDir = "data";
	cloudjs.spacing = spacing;
	cloudjs.outputFormat = OutputFormat::LAS;
}


AABB calculateAABB(vector<string> sources){
	AABB aabb;

	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		PointReader *reader = createPointReader(source);
		AABB lAABB = reader->getAABB();
		 

		aabb.update(lAABB.min);
		aabb.update(lAABB.max);

		reader->close();
		delete reader;
	}

	return aabb;
}

void PotreeConverter::convert(){
	AABB aabb;
	long long numPoints = 0;

	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		PointReader *reader = createPointReader(source);
		AABB lAABB = reader->getAABB();
		 

		aabb.update(lAABB.min);
		aabb.update(lAABB.max);

		numPoints += reader->numPoints();

		reader->close();
		delete reader;
	}







	cout << "Last level will have spacing:     " << spacing / pow(2, maxDepth - 1) << endl;
	cout << endl;

	cout << "AABB: " << endl << aabb << endl;

	if (diagonalFraction != 0) {
		spacing = (float)(aabb.size.length() / diagonalFraction);
		cout << "spacing calculated from diagonal: " << spacing << endl;
	}

	aabb.makeCubic();

	cout << "cubic AABB: " << endl << aabb << endl;

	cloudjs.boundingBox = aabb;

	auto start = high_resolution_clock::now();

	PotreeWriter *writer = NULL;
	if(algorithm == SelectionAlgorithm::NICE){
		writer = new PotreeWriterDartThrowing(this->workDir, aabb, spacing, maxDepth, scale, outputFormat);
	}else if(algorithm == SelectionAlgorithm::FAST){
		writer = new PotreeWriterRandom(this->workDir, numPoints, aabb, spacing, scale, outputFormat);
	}

	long long readTime = 0;
	long long pointsProcessed = 0;
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];
		cout << "OPEN:        " << source << endl;

		bool morePoints = true;
		PointReader *reader = createPointReader(source);
		while(morePoints){

			auto startRead = high_resolution_clock::now();
			morePoints = reader->readNextPoint();
			{
				auto endRead = high_resolution_clock::now();
				milliseconds duration = duration_cast<milliseconds>(endRead-startRead);
				readTime += duration.count();
			}
			

			if(!morePoints){
				break;
			}

			pointsProcessed++;

			Point p = reader->getPoint();

			writer->add(p);

			//if((pointsProcessed % (10*1000*1000)) == 0){
			//	writer->flush();
			//}

			if((pointsProcessed % (1*1000*1000)) == 0){
				auto end = high_resolution_clock::now();
				long duration = (long)duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1000.0f;

				stringstream ssMessage;
				ssMessage.imbue(std::locale(""));
				ssMessage << "INDEXING:    ";
				ssMessage << pointsProcessed << " read; ";
				ssMessage << writer->numPointsWritten() << " written; ";
				ssMessage << std::fixed << std::setprecision(1) << seconds << " seconds passed";

				cout << ssMessage.str() << endl;
			}
		}
		
		reader->close();
		delete reader;
	}
	writer->flush();

	cout << writer->numPointsWritten() << " points written" << endl;

	auto end = high_resolution_clock::now();
	long duration = (long)duration_cast<milliseconds>(end-start).count();
	cout << "duration: " << (duration / 1000.0f) << "s" << endl;
	
	cout << "closing writer" << endl;
	writer->close();
}
