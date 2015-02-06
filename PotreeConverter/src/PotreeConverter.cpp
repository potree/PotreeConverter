

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
	}else if(boost::iends_with(path, ".xyz")){
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

PotreeConverter::PotreeConverter(vector<string> sources, string workDir, float spacing, int diagonalFraction, int maxDepth, string format, vector<double> colorRange, vector<double> intensityRange, double scale, OutputFormat outFormat){

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
	this->workDir = workDir;
	this->spacing = spacing;
	this->maxDepth = maxDepth;
	this->format = format;
	this->colorRange = colorRange;
	this->intensityRange = intensityRange;
	this->scale = scale;
	this->outputFormat = outFormat;
	this->diagonalFraction = diagonalFraction;

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

void PotreeConverter::lasThread(list<string> &sources, list<int> &indexes, atomic<long long> &pointsProcessed) {
    auto start = high_resolution_clock::now();
    while (true) {
        popLasMutex.lock();
        if (sources.empty()) {
            popLasMutex.unlock();
            return;
        }
        string source = sources.back();
        int index = indexes.back();
        sources.pop_back();
        indexes.pop_back();
        popLasMutex.unlock();
        if (boost::iends_with(source, ".xyz") || boost::iends_with(source, ".pts") || boost::iends_with(source, ".ptx")) {
            boost::filesystem::path lasDir(workDir + "/las");
            boost::filesystem::create_directories(lasDir);
            string dest = workDir + "/las/" + fs::path(source).stem().string() + ".las";
            if (!boost::filesystem::exists(dest)) {
                PointReader *reader = createPointReader(source);
                LASPointWriter *writer = new LASPointWriter(dest, aabb, scale);
                AABB aabb;
                
                while (reader->readNextPoint()) {
                    Point p = reader->getPoint();
                    writer->write(p);
                    
                    Vector3<double> pos = p.position();
                    aabb.update(pos);
                    pointsProcessed.fetch_add(1);
                    
                    if (0 == pointsProcessed.storage() % FLUSH_INTERVAL) {
                        auto end = high_resolution_clock::now();
                        long long duration = duration_cast<milliseconds>(end - start).count();
                        float seconds = duration / 1000.0f;
                        stringstream ssMessage;
                        ssMessage.imbue(std::locale(""));
                        ssMessage << "CONVERT-LAS: ";
                        ssMessage << pointsProcessed.storage() << " points processed; ";
                        ssMessage << seconds << " seconds passed";
                        coutMutex.lock();
                        cout << ssMessage.str() << endl;
                        coutMutex.unlock();
                    } // if pointsProcessed
                } // while reader->readNextPoint()
                writer->header->SetMax(aabb.max.x, aabb.max.y, aabb.max.z);
                writer->header->SetMin(aabb.min.x, aabb.min.y, aabb.min.z);
                writer->writer->SetHeader(*writer->header);
                writer->writer->WriteHeader();
                
                reader->close();
                writer->close();
                
                delete reader;
                delete writer;
            } // if !exists
            
            this->sources[index] = dest;
        } // if extension
    }
}

void PotreeConverter::convert(){
	atomic<long long> pointsProcessed(0);
	auto start = high_resolution_clock::now();

    // convert XYZ sources to las sources
    list<string> sourcesList;
    list<int> indexList;
    for (int i = 0; i < sources.size(); i++) {
        sourcesList.push_back(sources[i]);
        indexList.push_back(i);
    }
    boost::thread_group group;
    for (int j = 0; j < THREADS_LAS; j++) {
        group.create_thread(boost::bind(&PotreeConverter::lasThread, this, boost::ref(sourcesList),
                                        boost::ref(indexList), boost::ref(pointsProcessed)));
    }
    group.join_all();

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

	PotreeWriter writer(this->workDir, aabb, spacing, maxDepth, scale, outputFormat);
	//PotreeWriterLBL writer(this->workDir, aabb, spacing, maxDepth, outputFormat);

    pointsProcessed.store(0);
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];
		cout << "reading " << source << endl;

		PointReader *reader = createPointReader(source);
		while(reader->readNextPoint()){
			pointsProcessed.fetch_add(1);
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

			if((pointsProcessed.storage() % (1000*1000)) == 0){
				writer.flush();

				auto end = high_resolution_clock::now();
				long long duration = duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1000.0f;

				stringstream ssMessage;

				ssMessage.imbue(std::locale(""));
				ssMessage << "INDEXING: ";
				ssMessage << pointsProcessed.storage() << " points processed; ";
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

	float percent = (float)writer.numAccepted / (float)pointsProcessed.storage();
	percent = percent * 100;

	auto end = high_resolution_clock::now();
	long long duration = duration_cast<milliseconds>(end-start).count();

	
	cout << endl;
	cout << "conversion finished" << endl;
	cout << pointsProcessed.storage() << " points were processed and " << writer.numAccepted << " points ( " << percent << "% ) were written to the output. " << endl;

	cout << "duration: " << (duration / 1000.0f) << "s" << endl;
}
