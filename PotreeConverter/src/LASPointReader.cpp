

#include <fstream>
#include <iostream>
#include <vector>

#include "lasfilter.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string.hpp>

#include "LASPointReader.h"


namespace fs = boost::filesystem;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using boost::iequals;

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
	for(int i = 0; i < files.size(); i++){
		string file = files[i];

		LASreadOpener readOpener;
		readOpener.set_file_name(file.c_str());

		LASreader *reader = readOpener.open();

		Vector3<double> min = Vector3<double>(reader->get_min_x(), reader->get_min_y(), reader->get_min_z());
		Vector3<double> max = Vector3<double>(reader->get_max_x(), reader->get_max_y(), reader->get_max_z());
		aabb.update(min);
		aabb.update(max);

		reader->close();
		delete reader;
	}

	// open first file
	currentFile = files.begin();
	LASreadOpener readOpener;
	readOpener.set_file_name(currentFile->c_str());
	reader = readOpener.open();
	

	//char first[] = "filter";
	//char second[] = "-keep_every_nth";
	//char third[] = "100";
	//char fourth[] = "\0";
	//
	//char* args[4];
	//args[0] = first;
	//args[1] = second;
	//args[2] = third;
	//args[3] = fourth;
	//
	//readOpener->parse(4, args);

	
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
	return reader->npoints;
}

bool LASPointReader::readNextPoint(){
	bool hasPoints = reader->read_point();

	if(!hasPoints){
		// try to open next file, if available
		reader->close();
		delete reader;
		reader = NULL;
		currentFile++;

		if(currentFile != files.end()){
			LASreadOpener readOpener;
			readOpener.set_file_name(currentFile->c_str());
			reader = readOpener.open();
			hasPoints = reader->read_point();
		}
	}

	return hasPoints;
}

Point LASPointReader::getPoint(){
	LASpoint &lp = reader->point;
	Point p(lp.get_x(), lp.get_y(), lp.get_z());

	p.intensity = lp.intensity;
	p.classification = lp.classification;
	
	const U16 *rgb = lp.get_rgb();
	p.r = rgb[0] / 256;
	p.g = rgb[1] / 256;
	p.b = rgb[2] / 256;


	return p;
}

AABB LASPointReader::getAABB(){
	return aabb;
}

Vector3<double> LASPointReader::getScale(){
	Vector3<double> scale;
	scale.x = getHeader().x_scale_factor;
	scale.y = getHeader().y_scale_factor;
	scale.z = getHeader().z_scale_factor;

	return scale;
}

LASheader const &LASPointReader::getHeader(){
	return reader->header;
}