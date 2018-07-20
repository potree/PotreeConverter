#include <experimental/filesystem>
#include "LocbPointReader.h"

#include "stuff.h"

namespace fs = std::experimental::filesystem;

namespace Potree{

LOCBPointReader::LOCBPointReader(string path){

	if(fs::is_directory(path)){
		// if directory is specified, find all las and laz files inside directory

		for(fs::directory_iterator it(path); it != fs::directory_iterator(); it++){
			fs::path filepath = it->path();
			if(fs::is_regular_file(filepath)){
				if(icompare(fs::path(filepath).extension().string(), ".locb")){
					files.push_back(filepath.string());
				}
			}
		}
	}else{
		files.push_back(path);
	}


	// read bounding box
	for (const auto &file : files) {
		reader = new LOCBReader(file);
		AABB lAABB = reader->getAABB();

		aabb.update(lAABB.min);
		aabb.update(lAABB.max);

		reader->close();
		delete reader;
		reader = NULL;
	}

	// open first file
	currentFile = files.begin();
	reader = new LOCBReader(*currentFile);
//    cout << "let's go..." << endl;
}

LOCBPointReader::~LOCBPointReader(){
	close();
}

void LOCBPointReader::close(){
	if(reader != NULL){
		reader->close();
		delete reader;
		reader = NULL;
	}
}

long long LOCBPointReader::numPoints(){
	long long points = reader->numPoints();
	return points;
}

bool LOCBPointReader::readNextPoint(){

	bool hasPoints = reader->readPoint();

	if(!hasPoints){
		// try to open next file, if available
		reader->close();
		delete reader;
		reader = NULL;

		currentFile++;

		if(currentFile != files.end()){
			reader = new LOCBReader(*currentFile);
			hasPoints = reader->readPoint();
		}
	}

	return hasPoints;
}

Point LOCBPointReader::getPoint(){
	Point const p = reader->GetPoint();
  //cout << p.position.x << ", " << p.position.y << ", " << p.position.z << endl;
    return p;
}

AABB LOCBPointReader::getAABB(){
	return aabb;
}

}
