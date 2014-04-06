

#ifndef XYZPOINTREADER_H
#define XYZPOINTREADER_H

#include "Point.h"
#include "PointReader.h"

#include <string>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>
#include <sstream>

#include "boost/assign.hpp"
#include "boost/algorithm/string.hpp"

using std::getline;
using std::ifstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::stringstream;
using namespace boost::assign;
using boost::split;
using boost::is_any_of;

class XYZPointReader : public PointReader{
private:
	AABB aabb;
	ifstream stream;
	long pointsRead;
	char *buffer;
	int pointByteSize;
	Point point;
	string format;
	float range;

public:
	XYZPointReader(string file, string format, float range)
	: stream(file, std::ios::in | std::ios::binary)
	{
		this->format = format;
		this->range = range;
		pointsRead = 0;
		
	}

	bool readNextPoint(){
		float x;
		float y;
		float z;
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a = 255;
		string line;
		if(getline(stream, line)){
			//vector<string> tokens = split(line, ' ');
			vector<string> tokens;
			split(tokens, line, is_any_of("\t "));
			for(int i = 0; i < format.size(); i++){
				string token = tokens[i];
				if(format[i] == 'x'){
					x = stof(token);
				}else if(format[i] == 'y'){
					y = stof(token);
				}else if(format[i] == 'z'){
					z = stof(token);
				}else if(format[i] == 'r'){
					r = 255.0f * stof(token) / range; 
				}else if(format[i] == 'g'){
					g = 255.0f * stof(token) / range; 
				}else if(format[i] == 'b'){
					b = 255.0f * stof(token) / range; 
				}else if(format[i] == 'i'){
					r = 255.0f * stof(token) / range;
					g = r;
					b = r;
				}
			}

			point = Point(x,y,z,r,g,b);
			pointsRead++;
			return true;
		}else{
			return false;
		}
	}

	Point getPoint(){
		return point;
	}

	AABB getAABB(){
		AABB aabb;

		return aabb;
	}

	long numPoints(){
		return -1;
	}

	void close(){
		stream.close();
	}


};



#endif