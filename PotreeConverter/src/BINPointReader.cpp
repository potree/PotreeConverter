

#include <fstream>
#include <iostream>
#include <vector>

#include "boost/filesystem.hpp"
#include <boost/algorithm/string.hpp>

#include "BINPointReader.hpp"

namespace fs = boost::filesystem;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using boost::iequals;
using std::ios;


BINPointReader::BINPointReader(string path,  AABB aabb, double scale, PointAttributes pointAttributes){
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
}

BINPointReader::~BINPointReader(){
		close();
}

void BINPointReader::close(){
	if(reader != NULL){
		reader->close();
		delete reader;
		reader = NULL;
	}
}

long BINPointReader::numPoints(){
	//TODO

	return 0;
}

bool BINPointReader::readNextPoint(){
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

	if(hasPoints){
		point = Point();
		char* buffer = new char[attributes.byteSize];
		reader->read(buffer, attributes.byteSize);

		if(!reader->good()){
            delete [] buffer;
			return false;
		}
		
		int offset = 0;
		for(int i = 0; i < attributes.size(); i++){
			const PointAttribute attribute = attributes[i];
			if(attribute == PointAttribute::POSITION_CARTESIAN){
				int* iBuffer = reinterpret_cast<int*>(buffer+offset);
				point.x = (iBuffer[0] * scale) + aabb.min.x;
				point.y = (iBuffer[1] * scale) + aabb.min.y;
				point.z = (iBuffer[2] * scale) + aabb.min.z;
			}else if(attribute == PointAttribute::COLOR_PACKED){
				unsigned char* ucBuffer = reinterpret_cast<unsigned char*>(buffer+offset);
				point.r = ucBuffer[0];
				point.g = ucBuffer[1];
				point.b = ucBuffer[2];
				point.a = 255;
			}else if(attribute == PointAttribute::INTENSITY){
				unsigned short* usBuffer = reinterpret_cast<unsigned short*>(buffer+offset);
				point.intensity = usBuffer[0];
			}else if(attribute == PointAttribute::CLASSIFICATION){
				unsigned char* ucBuffer = reinterpret_cast<unsigned char*>(buffer+offset);
				point.classification = ucBuffer[0];
			}else if(attribute == PointAttribute::NORMAL_SPHEREMAPPED){
				// see http://aras-p.info/texts/CompactNormalStorage.html
				unsigned char* ucBuffer = reinterpret_cast<unsigned char*>(buffer+offset);
				unsigned char bx = ucBuffer[0];
				unsigned char by = ucBuffer[1];

 				float ex = (float)bx / 255.0f;
				float ey = (float)by / 255.0f;

				float nx = ex * 2 - 1;
				float ny = ey * 2 - 1;
				float nz = 1;
				float nw = -1;

				float l = (nx * (-nx) + ny * (-ny) + nz * (-nw));
				nz = l;
				nx = nx * sqrt(l);
				ny = ny * sqrt(l);

				nx = nx * 2;
				ny = ny * 2;
				nz = nz * 2 -1;

				point.nx = nx;
				point.ny = ny;
				point.nz = nz;

			}
			offset += attribute.byteSize;
		}
		
		delete [] buffer;
	}

	return hasPoints;
}

Point BINPointReader::getPoint(){
	return point;
}

AABB BINPointReader::getAABB(){
	AABB aabb;
	//TODO

	return aabb;
}








