

#include "LASPointReader.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "lasfilter.hpp"


using std::ifstream;
using std::cout;
using std::endl;
using std::vector;

LASPointReader::LASPointReader(string file){
	this->file = file;
	readOpener = new LASreadOpener();
	readOpener->set_file_name(file.c_str());
	

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

	reader = readOpener->open();

	Vector3<double> min = Vector3<double>(reader->get_min_x(), reader->get_min_y(), reader->get_min_z());
	Vector3<double> max = Vector3<double>(reader->get_max_x(), reader->get_max_y(), reader->get_max_z());
	aabb = AABB(min, max);
}

void LASPointReader::close(){
	reader->close();
}

long LASPointReader::numPoints(){
	return reader->npoints;
}

bool LASPointReader::readNextPoint(){
	return reader->read_point();
}

Point LASPointReader::getPoint(){
	LASpoint &lp = reader->point;
	Point p(lp.get_x(), lp.get_y(), lp.get_z());

	p.intensity = lp.intensity;
	
	//const U16 *rgb = lp.get_rgb();
	//p.r = rgb[0] / 65025.0f;
	//p.g = rgb[1] / 65025.0f;
	//p.b = rgb[2] / 65025.0f;

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