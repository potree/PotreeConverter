

#include <fstream>
#include <iostream>
#include <vector>

#include "boost/filesystem.hpp"
#include <boost/algorithm/string.hpp>
#include <liblas/detail/reader/reader.hpp>

#include "LASPointReader.h"


namespace fs = boost::filesystem;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using boost::iequals;
using std::ios;
using liblas::VariableRecord;

AABB LIBLASReader::getAABB(){
    AABB aabb;

    const liblas::Header &header = reader.GetHeader();

    Point minp = transform(header.GetMinX(), header.GetMinY(), header.GetMinZ());
    Point maxp = transform(header.GetMaxX(), header.GetMaxY(), header.GetMaxZ());

    aabb.update(minp.position);
    aabb.update(maxp.position);

    return aabb;
}

LASPointReader::LASPointReader(string path){
	this->path = path;

	
	if(fs::is_directory(path)){
		// if directory is specified, find all las and laz files inside directory

		for(fs::directory_iterator it(path); it != fs::directory_iterator(); it++){
			fs::path filepath = it->path();
			if(fs::is_regular_file(filepath)){
				if(iequals(fs::extension(filepath), ".las") || iequals(fs::extension(filepath), ".laz")){
					files.push_back(filepath.string());
				}
			}
		}
	}else{
		files.push_back(path);
	}
	

	// read bounding box
	for (const auto &file : files) {
		LIBLASReader aabbReader(file);
		AABB lAABB = aabbReader.getAABB();
		
		aabb.update(lAABB.min);
		aabb.update(lAABB.max);

		aabbReader.close();
	}

	// open first file
	currentFile = files.begin();
	reader = new LIBLASReader(*currentFile);
//    cout << "let's go..." << endl;
}

LASPointReader::~LASPointReader(){
	close();
}

void LASPointReader::close(){
	if(reader != NULL){
		reader->close();
		delete reader;
		reader = NULL;
	}
}

long LASPointReader::numPoints(){
	return reader->reader.GetHeader().GetPointRecordsCount();
}

bool LASPointReader::readNextPoint(){
	bool hasPoints = reader->reader.ReadNextPoint();

	if(!hasPoints){
		// try to open next file, if available
		reader->close();
		delete reader;
		reader = NULL;

		currentFile++;

		if(currentFile != files.end()){
			reader = new LIBLASReader(*currentFile);
			hasPoints = reader->reader.ReadNextPoint();
		}
	}

	return hasPoints;
}

Point LASPointReader::getPoint(){
	Point const p = reader->GetPoint();
    return p;
}

AABB LASPointReader::getAABB(){
	return aabb;
}

Vector3<double> LASPointReader::getScale(){

	Vector3<double> scale;
	scale.x = reader->reader.GetHeader().GetScaleX();
	scale.y = reader->reader.GetHeader().GetScaleY();
	scale.z = reader->reader.GetHeader().GetScaleZ();

	return scale;
}
