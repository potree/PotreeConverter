#ifndef XYZPOINTREADER_H
#define XYZPOINTREADER_H

#include "Point.h"
#include "PointReader.h"
#include "PotreeException.h"

#include <string>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>
#include <sstream>
#include <algorithm>

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
using boost::token_compress_on;
using boost::is_any_of;
using boost::trim;
using boost::erase_all;

class XYZPointReader : public PointReader{
private:
	AABB aabb;
	ifstream stream;
	long pointsRead;
	char *buffer;
	int pointByteSize;
	Point point;
	string format;

	float colorOffset;
	float colorScale;

	float intensityOffset;
	float intensityScale;

	int linesSkipped;

public:
	XYZPointReader(string file, string format, vector<double> colorRange, vector<double> intensityRange)
	: stream(file, std::ios::in | std::ios::binary)
	{
		this->format = format;
		pointsRead = 0;
		linesSkipped = 0;

		if(intensityRange.size() == 2){
			intensityOffset = intensityRange[0];
			intensityScale = intensityRange[1];
		}else if(intensityRange.size() == 1){
			intensityOffset = 0.0f;
			intensityScale = intensityRange[0];
		}else{
			intensityOffset = 0.0f;
			intensityScale = 1.0f;
		}

		if(colorRange.size() == 2){
			colorOffset = colorRange[0];
			colorScale = colorRange[1];
		}else if(colorRange.size() == 1){
			colorOffset = 0.0f;
			colorScale = colorRange[0];
		}else if(colorRange.size() == 0){
			colorOffset = 0.0f;

			// try to find color range by evaluating the first x points.
			float max;
			int j = 0; 
			string line;
			while(getline(stream, line) && j < 1000){
				trim(line);
				vector<string> tokens;
				split(tokens, line, is_any_of("\t ,"), token_compress_on); 

				if(this->format == "" && tokens.size() >= 3){
					string f(tokens.size(), 's');
					f.replace(0, 3, "xyz");

					if(tokens.size() >= 6){
						f.replace(tokens.size() - 3, 3, "rgb");
					}

					this->format = f;
					cout << "using format: '" << this->format << "'" << endl;
				}

				if(tokens.size() < this->format.size()){
					continue;
				}

				for(int i = 0; i < this->format.size(); i++){
					string token = tokens[i];
					auto f = this->format[i];
					if(f == 'r'){
						max = std::max(max, stof(token));
					}else if(f == 'g'){
						max = std::max(max, stof(token));
					}else if(f == 'b'){
						max = std::max(max, stof(token));
					}
				}

				j++;
			}

			if(max <= 1.0f){
				colorScale = 1.0f;
			} else if(max <= 255){
				colorScale = 255.0f;
			}else if(max <= pow(2, 16) - 1){
				colorScale =(float)pow(2, 16) - 1;
			}else{
				colorScale = (float)max;
			}

			stream.seekg(0, stream.beg);

		}

	}

	bool readNextPoint(){
		double x;
		double y;
		double z;
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a = 255;
		unsigned short intensity = 0;

		string line;
		while(getline(stream, line)){
			trim(line);
			vector<string> tokens;
			split(tokens, line, is_any_of("\t ,"), token_compress_on); 
			if(tokens.size() < format.size() && linesSkipped == 0){
				//throw PotreeException("Not enough tokens for the given format");
				cout << "some lines may be skipped because they do not match the given format: '" << format << "'" << endl;
				linesSkipped++;
				continue;
			}

			for(int i = 0; i < format.size(); i++){
				string token = tokens[i];
				auto f = format[i];
				if(f == 'x'){
					x = stod(token);
				}else if(f == 'y'){
					y = stod(token);
				}else if(f == 'z'){
					z = stod(token);
				}else if(f == 'r'){
					r = (unsigned char)(255.0f * (stof(token) - colorOffset) / colorScale); 
				}else if(f == 'g'){
					g = (unsigned char)(255.0f * (stof(token) - colorOffset) / colorScale); 
				}else if(f == 'b'){
					b = (unsigned char)(255.0f * (stof(token) - colorOffset) / colorScale); 
				}else if(f == 'i'){
					intensity = (unsigned short)( 65535 * (stof(token) - intensityOffset) / intensityScale);
				}else if(f == 's'){
					// skip
				}
			}

			point = Point(x,y,z,r,g,b);
			point.intensity = intensity;
			pointsRead++;
			return true;
		}

		return false;
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