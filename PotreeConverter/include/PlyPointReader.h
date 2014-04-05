

#ifndef PLYPOINTREADER_H
#define PLYPOINTREADER_H

#include "Point.h"
#include "PointReader.h"

#include <string>
#include <fstream>
#include <iostream>
#include <regex>

#include "boost/assign.hpp"

using std::ifstream;
using std::string;
using std::cout;
using std::endl;
using namespace boost::assign;

const int PLY_FILE_FORMAT_ASCII = 0;
const int PLY_FILE_FORMAT_BINARY_LITTLE_ENDIAN = 1;


struct PlyPropertyType{
	string name;
	int size;

	PlyPropertyType(){}

	PlyPropertyType(string name, int size)
	:name(name)
	,size(size)
	{
	
	}
};

struct PlyProperty{
	string name;
	PlyPropertyType type;

	PlyProperty(string name, PlyPropertyType type)
		:name(name)
		,type(type)
	{

	}
};

struct PlyElement{
	string name;
	vector<PlyProperty> properties;
	int size;

	PlyElement(string name)
		:name(name)
	{

	}
};


map<string, PlyPropertyType> plyPropertyTypes = map_list_of
	("char", PlyPropertyType("char", 1))
	("uchar", PlyPropertyType("uchar", 1))
	("short", PlyPropertyType("short", 2))
	("ushort", PlyPropertyType("ushort", 2))
	("int", PlyPropertyType("int", 4))
	("uint", PlyPropertyType("uint", 4))
	("float", PlyPropertyType("float", 4))
	("double", PlyPropertyType("double", 8));

vector<string> plyRedNames = list_of("r")("red")("diffuse_red");
vector<string> plyGreenNames = list_of("g")("green")("diffuse_green");
vector<string> plyBlueNames = list_of("b")("blue")("diffuse_blue");

class PlyPointReader : public PointReader{
private:
	AABB aabb;
	ifstream stream;
	int format;
	long pointCount;
	long pointsRead;
	PlyElement vertexElement;
	char *buffer;
	int pointByteSize;
	Point point;

public:
	PlyPointReader(string file)
	: stream(file, std::ios::in | std::ios::binary)
	,vertexElement("vertexElement"){
		format = -1;
		pointCount = 0;
		pointsRead = 0;
		buffer = new char[100];

		std::regex rEndHeader("^end_header.*");
		std::regex rFormat("^format (ascii|binary_little_endian).*");
		std::regex rElement("^element (\\w*) (\\d*)");
		std::regex rProperty("^property (char|uchar|short|ushort|int|uint|float|double) (\\w*)");
		
		string line;
		while(std::getline(stream, line)){
			cout << line << endl;

			std::smatch sm;
			if(regex_match(line, rEndHeader)){
				// stop line parsing when end_header is encountered
				break;
			}else if(regex_match(line, sm, rFormat)){
				// parse format
				string f = sm[1];
				if(f == "ascii"){
					format = PLY_FILE_FORMAT_ASCII;
				}else if(f == "binary_little_endian"){
					format = PLY_FILE_FORMAT_BINARY_LITTLE_ENDIAN;
				}
			}else if(regex_match(line, sm, rElement)){
				// parse vertex element declaration
				string name = sm[1];
				long count = atol(string(sm[2]).c_str());

				if(name != "vertex"){
					cout << "As of now, only ply files with 'vertex' as the first element are supported." << endl;
					continue;
				}
				pointCount = count;

				while(true){
					int len = stream.tellg();
					getline(stream, line);
					if(regex_match(line, sm, rProperty)){
						string name = sm[2];
						PlyPropertyType type = plyPropertyTypes[sm[1]];
						PlyProperty property(name, type);
						vertexElement.properties.push_back(property);
						pointByteSize += type.size;
					}else{
						// abort if line was not a property definition
						stream.seekg(len ,std::ios_base::beg);
						break;
					}
				}
			}
		}
	}

	bool readNextPoint(){
		if(pointsRead == pointCount){
			return false;
		}
		
		float x,y,z;
		unsigned char r,g,b;

		if(format == PLY_FILE_FORMAT_ASCII){
			string line;
			getline(stream, line);
		}else if(format == PLY_FILE_FORMAT_BINARY_LITTLE_ENDIAN){
			stream.read(buffer, pointByteSize);

			int offset = 0;
			for(int i = 0; i < vertexElement.properties.size(); i++){
				PlyProperty prop = vertexElement.properties[i];
				if(prop.name == "x" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&x, (buffer+offset), prop.type.size);
				}else if(prop.name == "y" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&y, (buffer+offset), prop.type.size);
				}else if(prop.name == "z" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&z, (buffer+offset), prop.type.size);
				}else if(std::find(plyRedNames.begin(), plyRedNames.end(), prop.name) != plyRedNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					memcpy(&r, (buffer+offset), prop.type.size);
				}else if(std::find(plyGreenNames.begin(), plyGreenNames.end(), prop.name) != plyGreenNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					memcpy(&g, (buffer+offset), prop.type.size);
				}else if(std::find(plyBlueNames.begin(), plyBlueNames.end(), prop.name) != plyBlueNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					memcpy(&b, (buffer+offset), prop.type.size);
				}
				

				offset += prop.type.size;
			}
			
		}

		point = Point(x,y,z,r,g,b);
		pointsRead++;
		return true;
	}

	Point getPoint(){
		return point;
	}

	AABB getAABB(){
		AABB aabb;

		return aabb;
	}

	long numPoints(){
		return pointCount;
	}

	void close(){
		stream.close();
	}


};



#endif