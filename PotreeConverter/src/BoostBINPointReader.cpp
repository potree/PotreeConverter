
#include <fstream>
#include <iostream>
#include <vector>


#include <experimental/filesystem>

#include "BoostBINPointReader.hpp"

#include "stuff.h"

namespace fs = std::experimental::filesystem;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using std::ios;

namespace Potree{

BoostBINPointReader::BoostBINPointReader(string path, AABB aabb, double scale, PointAttributes pointAttributes) : endOfFile(false) {
	this->path = path;
	this->aabb = aabb;
	this->scale = scale;
	this->attributes = pointAttributes;

	if(fs::is_directory(path)){
		// if directory is specified, find all las and laz files inside directory

		for(fs::directory_iterator it(path); it != fs::directory_iterator(); it++){
			fs::path filepath = it->path();
			if(fs::is_regular_file(filepath)){
				files.push_back(filepath.string());
			}
		}
	}else{
		files.push_back(path);
	}

	currentFile = files.begin();
	reader = new ifstream(*currentFile, ios::in | ios::binary);
	archivePtr = std::unique_ptr<boost::archive::binary_iarchive>(new boost::archive::binary_iarchive(*reader));

	// Calculate AABB:
	if (true) {
		pointCount = 0;
		while(readNextPoint()){
			Point p = getPoint();
			if (pointCount == 0) {
				this->aabb = AABB(p.position);
			} else {
				this->aabb.update(p.position);
			}
			pointCount++;
		}
		reader->clear();
		reader->seekg(0, reader->beg);

		reader = new ifstream(*currentFile, ios::in | ios::binary);
		archivePtr = std::unique_ptr<boost::archive::binary_iarchive>(new boost::archive::binary_iarchive(*reader));
	}

}

BoostBINPointReader::~BoostBINPointReader(){
		close();
}

void BoostBINPointReader::close(){
	archivePtr.reset();

	if(reader != NULL){
		reader->close();
		delete reader;
		reader = NULL;
	}
}

long long BoostBINPointReader::numPoints(){
	return pointCount;
}

bool BoostBINPointReader::readNextPoint(){
	bool hasPoints = reader->good();


	if(!hasPoints){
		// try to open next file, if available
		reader->close();
		delete reader;
		reader = NULL;
		currentFile++;

		if(currentFile != files.end()){
			reader = new ifstream(*currentFile, ios::in | ios::binary);
			hasPoints = reader->good();
		}
	}

	if(hasPoints && !endOfFile){
		this->point = Point();
		char* buffer = new char[attributes.byteSize];
		// reader->read(buffer, attributes.byteSize);

		try {
			LidarScan scan = {};
			*archivePtr >> scan;

			// std::cout << "SCAN: " << scan << std::endl;

			point.position.x = scan.easting;
			point.position.y = scan.northing;
			point.position.z = scan.altitude;
			point.intensity = scan.intensity;
			point.gpsTime = scan.gpsTime;

			// std::cout << "POINT: "
			// 					<< point.position.x << " >< "
			// 					<< point.position.y << " >< "
			// 					<< point.position.z << " >< "
			// 					<< point.intensity << " >< "
			// 					<< point.gpsTime
			// 					<< std::endl;
			delete [] buffer;
			return true;

		} catch (std::exception& e) {
			endOfFile = false;
			std::cout << "END OF FILE REACHED" << std::endl;
			delete [] buffer;
			return false;
		}


	}

	return hasPoints;
}

Point BoostBINPointReader::getPoint(){
	return point;
}

AABB BoostBINPointReader::getAABB(){
	return aabb;
}

}
