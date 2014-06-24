


#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "BinPointReader.h"
#include "PlyPointReader.h"
#include "XYZPointReader.h"
#include "PotreeException.h"

#include <liblas/liblas.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <math.h>

using std::stringstream;
using std::map;
using std::string;
using std::vector;
using std::find;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using boost::iends_with;

long addAncestorTime = 0;
long addSourceTime = 0;
long saveCloudJSTime = 0;

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

PotreeConverter::PotreeConverter(vector<string> fData, string workDir, float minGap, int maxDepth, string format, float range){
	for(int i = 0; i < fData.size(); i++){
		if(!boost::filesystem::exists(fData[i])){
			throw PotreeException("file not found: " + fData[i]);
		}
	}
	

	this->fData = fData;
	this->workDir = workDir;
	this->minGap = minGap;
	this->maxDepth = maxDepth;
	this->format = format;
	this->range = range;
	buffer = new char[4*10*1000*1000*sizeof(float)];

	boost::filesystem::path dataDir(workDir + "/data");
	boost::filesystem::path tempDir(workDir + "/temp");
	boost::filesystem::create_directories(dataDir);
	boost::filesystem::create_directories(tempDir);

	initReader();
}

void PotreeConverter::initReader(){

	for(int i = 0; i < fData.size(); i++){
		string fname = fData[i];
		PointReader *reader = NULL;
		if(iends_with(fname, ".las") || iends_with(fname, ".laz")){
			cout << "creating LAS reader" << endl;
			reader = new LASPointReader(fname);
		}else if(iends_with(fname, ".bin")){
			cout << "creating bin reader" << endl;
			reader = new BinPointReader(fname);
		}else if(iends_with(fname, ".ply")){
			cout << "creating ply reader" << endl;
			PlyPointReader *plyreader = new PlyPointReader(fname);

			cout << "converting ply file to bin file" << endl;
			string binpath = workDir + "/temp/bin";
			ofstream sout(binpath, ios::out | ios::binary | ios::app);
			while(plyreader->readNextPoint()){
				Point p = plyreader->getPoint();
				sout.write((const char*)&p, sizeof(Point));
			}
			plyreader->close();
			delete plyreader;

			cout << "saved bin to " << binpath << endl;
			cout << "creating bin reader" << endl;
			reader = new BinPointReader(binpath);
		}else if(iends_with(fname, ".xyz") || iends_with(fname, ".txt")){
			cout << "creating xyz reader" << endl;
			XYZPointReader *xyzreader = new XYZPointReader(fname, format, range);

			cout << "converting xyz file to bin file" << endl;
			string binpath = workDir + "/temp/bin";
			ofstream sout(binpath, ios::out | ios::binary | ios::app);
			while(xyzreader->readNextPoint()){
				Point p = xyzreader->getPoint();
				sout.write((const char*)&p, sizeof(Point));
			}
			xyzreader->close();
			delete xyzreader;

			cout << "saved bin to " << binpath << endl;
			cout << "creating bin reader" << endl;
			reader = new BinPointReader(binpath);
		}else{
			throw PotreeException("filename did not match a known format");
		}

		if(reader != NULL){
			this->reader.insert(std::make_pair(fname, reader));
		}
	}

	currentReader = reader.begin();
	
}

void PotreeConverter::convert(){
	uint64_t numPoints = 0;
	map<string, PointReader*>::iterator it;
	for(it = reader.begin(); it != reader.end(); it++){
		numPoints += it->second->numPoints();
	}
	convert(numPoints);
}

bool PotreeConverter::readNextPoint(){
	bool success = currentReader->second->readNextPoint();

	if(!success){
		currentReader++;
		if(currentReader == reader.end()){
			return false;
		}

		success = currentReader->second->readNextPoint();
	}

	return success;
}

Point PotreeConverter::getPoint(){
	return currentReader->second->getPoint();
}

void PotreeConverter::convert(uint64_t numPoints){

	{ // calculate aabb
		Vector3 min(std::numeric_limits<float>::max());
		Vector3 max(-std::numeric_limits<float>::max());
		map<string, PointReader*>::iterator it;
		for(it = reader.begin(); it != reader.end(); it++){
			AABB box = it->second->getAABB();
			min.x = std::min(min.x, box.min.x);
			min.y = std::min(min.y, box.min.y);
			min.z = std::min(min.z, box.min.z);
			max.x = std::max(max.x, box.max.x);
			max.y = std::max(max.y, box.max.y);
			max.z = std::max(max.z, box.max.z);
		}
		aabb = AABB(min, max);

		cout << "AABB: " << endl;
		cout << aabb << endl;
	}
	
	{ // check dimension
		if(minGap == 0.0f){
			double volume = aabb.size.x * aabb.size.y * aabb.size.z;
			minGap = pow(volume, 1.0 / 3.0) * 0.005;
			cout << "automatically calculated spacing: " << minGap << endl;
		}

		double threshold = 10*1000;
		double width = aabb.size.x / minGap;
		double height = aabb.size.y / minGap;
		double depth = aabb.size.z / minGap;
		
		if(width > threshold || height > threshold || depth > threshold){
			cout << endl;
			cout << "WARNING: It seems that either your bounding box is too large or your spacing too small." << endl;
			cout << "Conversion might not work" << endl;
			cout << endl;
		}

	}

	cloudJs.clear();
	cloudJs << "{" << endl;
	cloudJs << "\t" << "\"version\": \"" << POTREE_FORMAT_VERSION << "\"," << endl;
	cloudJs << "\t" << "\"octreeDir\": \"data\"," << endl;
	cloudJs << "\t" << "\"boundingBox\": {" << endl;
	cloudJs << "\t\t" << "\"lx\": " << aabb.min.x << "," << endl;
	cloudJs << "\t\t" << "\"ly\": " << aabb.min.y << "," << endl;
	cloudJs << "\t\t" << "\"lz\": " << aabb.min.z << "," << endl;
	cloudJs << "\t\t" << "\"ux\": " << aabb.max.x << "," << endl;
	cloudJs << "\t\t" << "\"uy\": " << aabb.max.y << "," << endl;
	cloudJs << "\t\t" << "\"uz\": " << aabb.max.z << endl;
	cloudJs << "\t" << "}," << endl;
	cloudJs << "\t" << "\"pointAttributes\": [" << endl;
	cloudJs << "\t\t" << "\"POSITION_CARTESIAN\"," << endl;
	cloudJs << "\t\t" << "\"COLOR_PACKED\"" << endl;
	cloudJs << "\t" << "]," << endl;
	cloudJs << "\t" << "\"spacing\": " << minGap << "," << endl;
	cloudJs << "\t" << "\"hierarchy\": [" << endl;

	uint64_t numAccepted = 0;
	uint64_t numRejected = 0;
	{ // handle root
		cout << "processing level 0, points: " << numPoints << endl;
		SparseGrid grid(aabb, minGap);
		uint64_t pointsRead = 0;

		ofstream srOut(workDir + "/data/r", ios::out | ios::binary);
		ofstream sdOut(workDir + "/temp/d", ios::out | ios::binary);
		
		float minGapAtMaxDepth = minGap / pow(2.0f, maxDepth);
		uint64_t i = 0;
		while(readNextPoint()){
			Point p = getPoint();

			bool accepted = grid.add(p);
			int index = nodeIndex(aabb, p);
			if(accepted){
				// write point to ./data/r-file
				srOut.write((const char*)&p, sizeof(Point));
				numAccepted++;
			}else{
				// write point to ./temp/d-file
				sdOut.write((const char*)&p, sizeof(Point));
				numRejected++;
			}
			i++;
		}
		cloudJs << "\t\t" << "[\"r\"," << numAccepted << "]," << endl;
		saveCloudJS();

		srOut.close();
		sdOut.close();
	}

	// close readers
	map<string, PointReader*>::iterator it;
	for(it = reader.begin(); it != reader.end(); it++){
		it->second->close();
	}

	string source = workDir + "/temp/d";
	string target = workDir + "/data/r";
	int depth = 1;
	vector<Task> work;
	uint64_t unprocessedPoints = numRejected;
	uint64_t processedPoints = numAccepted;
	work.push_back(Task(source, target, aabb));

	// process points in breadth first order
	while(!work.empty() && depth <= maxDepth){
		cout << "processing level " << depth << ", points: " << unprocessedPoints << endl;
		vector<Task> nextRound;
		unprocessedPoints = 0;
		processedPoints = 0;

		vector<Task>::iterator it;
		for(it = work.begin(); it != work.end(); it++){
			source = it->source;
			target = it->target;
			AABB aabb = it->aabb;

			ProcessResult result = process(source, target, aabb, depth);

			// prepare the workload of the next level
			unprocessedPoints += result.numRejected;
			processedPoints += result.numAccepted;
			for(int i = 0; i < result.indices.size(); i++){
				int index = result.indices[i];
				stringstream ssSource, ssTarget;
				ssSource << source << result.indices[i];
				ssTarget << target << result.indices[i];
				AABB chAABB = childAABB(aabb, index);
				Task task(ssSource.str(), ssTarget.str(), chAABB);
				nextRound.push_back(task);
			}
		}

		depth++;
		work = nextRound;
	}

	{ // print number of processed points
		float percentage = float(numPoints - unprocessedPoints) / float(numPoints);
		cout << (numPoints - unprocessedPoints) << " of " << numPoints << " points were processed - ";
		cout << int(percentage*100) << "%" << endl;
	}


	stringstream ssCloudJs;
	string strCloudJs = cloudJs.str();
	ssCloudJs << strCloudJs.erase(strCloudJs.find_last_of(",")) << endl;
	ssCloudJs << "\t]" << endl;
	ssCloudJs << "}" << endl;

	ofstream sCloudJs(workDir + "/cloud.js", ios::out);
	sCloudJs << ssCloudJs.str();
	sCloudJs.close();

	delete[] buffer;

	//float addAncestorDuration = float(addAncestorTime)/1000.0f;
	//float addSourceDuration = float(addSourceTime)/1000.0f;
	//float saveCloudJSDuration = float(saveCloudJSTime)/1000.0f;
	//cout << "addAncestorDuration: " << addAncestorDuration << "s" << endl;
	//cout << "addSourceDuration: " << addSourceDuration << "s" << endl;
	//cout << "saveCloudJSDuration: " << saveCloudJSDuration << "s" << endl;
}

/**
 * 1. load all points from upper levels (r, rx, rxy, until r...depth) and add them to the grid 
 * 2. load points from source and add them to the grid
 * 3. accept all points from source which have a minimum distance of minGap / 2^depth to all 
 *    points inside the grid
 * 4. save accepted points of source to target + index and the remaining points to source + index
 *
 * @returns the indices of the nodes that have been created 
 *  if nodes [r11, r13, r17] were created, then indices [1,3,7] will be returned
 *
 **/
ProcessResult PotreeConverter::process(string source, string target, AABB aabb, int depth){
	vector<int> indices;
	uint64_t numAccepted = 0;
	uint64_t numRejected = 0;
	
	{
		stringstream ssName;
		ssName << source.substr(source.find_last_of("d"), source.size() - source.find_last_of("d"));
		stringstream ssAABB;
		ssAABB << workDir << "/aabb/" << ssName.str();
		ofstream fAABB(ssAABB.str(), ios::out);
		fAABB << "min: " << aabb.min << endl;
		fAABB << "max: " << aabb.max << endl;
		fAABB.close();
	}

	SparseGrid grid(aabb, minGap / pow(2.0, depth));

	{// add all ancestors to the grid
		auto start = high_resolution_clock::now();

		int pos = target.find_last_of("r");
		int numAncestors = target.size() - pos;
		string ancestor = target;
		for(int i = 0; i < numAncestors; i++){
			ifstream sIn(ancestor, ios::in | ios::binary);
			uint64_t pointsRead = 0;
			int batchSize = 10*1000*1000;
			int batchByteSize = 4*batchSize*sizeof(float);
			float *points = reinterpret_cast<float*>(buffer);
			char *cPoints = buffer;
			bool done = false;
			while(!done){
				sIn.read(buffer, batchByteSize);
				int pointsReadRightNow = sIn.gcount() / (4*sizeof(float));
				done = pointsReadRightNow == 0;

				for(int i = 0; i < pointsReadRightNow; i++){
					float x = points[4*i+0];
					float y = points[4*i+1];
					float z = points[4*i+2];
					char r = cPoints[16*i+12];
					char g = cPoints[16*i+13];
					char b = cPoints[16*i+14];
					Point p(x,y,z, r, g, b);
					if(aabb.isInside(p)){
						grid.addWithoutCheck(p);
					}
				}
			}

			ancestor = ancestor.substr(0, ancestor.size()-1);
		}
		auto end = high_resolution_clock::now();
		addAncestorTime += duration_cast<milliseconds>(end-start).count();
	}

	{ // add source to the grid
		auto start = high_resolution_clock::now();
		ifstream sIn(source, ios::in | ios::binary);
		uint64_t pointsRead = 0;
		int batchSize = 10*1000*1000;
		int batchByteSize = 4*batchSize*sizeof(float);
		float *points = reinterpret_cast<float*>(buffer);
		char *cPoints = buffer;
		bool done = false;
		vector<ofstream*> srOut;
		vector<ofstream*> sdOut;
		vector<uint64_t> numPoints;
		numPoints.resize(8);
		for(int i = 0; i < 8; i++){
			stringstream ssr, ssd;
			ssr << target << i;
			ssd << source << i;
			srOut.push_back(new ofstream(ssr.str(), ios::out | ios::binary));
			sdOut.push_back(new ofstream(ssd.str(), ios::out | ios::binary));
		}
		long pointsProcessed = 0;
		while(!done){
			sIn.read(buffer, batchByteSize);
			int pointsReadRightNow = sIn.gcount() / (4*sizeof(float));
			done = pointsReadRightNow == 0;

			for(int i = 0; i < pointsReadRightNow; i++){
				
				float x = points[4*i+0];
				float y = points[4*i+1];
				float z = points[4*i+2];
				char r = cPoints[16*i+12];
				char g = cPoints[16*i+13];
				char b = cPoints[16*i+14];
				Point p(x,y,z, r, g, b);

				bool accepted = grid.add(p);
				int index = nodeIndex(aabb, p);
				if(index == -1){
					continue;
				}

				if(accepted){
					srOut[index]->write((const char*)&p, sizeof(Point));
					if(find(indices.begin(), indices.end(), index) == indices.end()){
						indices.push_back(index);
					}
					numPoints[index]++;
					numAccepted++;
				}else{
					sdOut[index]->write((const char*)&p, sizeof(Point));
					numRejected++;
				}
				pointsProcessed++;
			}
		}

		for(int i = 0; i < 8; i++){
			srOut[i]->close();
			delete srOut[i];
			sdOut[i]->close();
			delete sdOut[i];

			{// remove empty files
				stringstream ssr, ssd;
				ssr << target << i;
				ssd << source << i;
				if(filesize(ssr.str()) == 0){
					remove(ssr.str().c_str());
				}
				if(filesize(ssd.str()) == 0){
					remove(ssd.str().c_str());
				}
			}


			if(numPoints[i] > 0){
				stringstream ssName;
				ssName << target.substr(target.find_last_of("r"), target.size() - target.find_last_of("r")) << i;
			}
		}

		auto end = high_resolution_clock::now();
		addSourceTime += duration_cast<milliseconds>(end-start).count();

		{ // update cloud.js
			auto start = high_resolution_clock::now();

			for(int i = 0; i < 8; i++){
				if(numPoints[i] > 0){
					stringstream ssName;
					ssName << target.substr(target.find_last_of("r"), target.size() - target.find_last_of("r")) << i;
					cloudJs << "\t\t" << "[\"" << ssName.str() << "\", " << numPoints[i] << "],";
					cloudJs << endl;
				}
			}
			saveCloudJS();
		}
	}

	return ProcessResult(indices, numAccepted, numRejected);
}


void PotreeConverter::saveCloudJS(){
	stringstream ssCloudJs;
	string strCloudJs = cloudJs.str();
	ssCloudJs << strCloudJs.erase(strCloudJs.find_last_of(",")) << endl;
	ssCloudJs << "\t]" << endl;
	ssCloudJs << "}" << endl;

	ofstream sCloudJs(workDir + "/cloud.js", ios::out);
	sCloudJs << ssCloudJs.str();
	sCloudJs.close();
}