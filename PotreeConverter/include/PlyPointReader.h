

#ifndef PLYPOINTREADER_H
#define PLYPOINTREADER_H

#include <string>
#include <fstream>
#include <iostream>
#include <regex>

#include "Point.h"
#include "PointReader.h"

using std::ifstream;
using std::string;
using std::vector;
using std::map;
using std::cout;
using std::endl;

namespace Potree{

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

unordered_map<string, PlyPropertyType> plyPropertyTypes = { 
	{ "char", PlyPropertyType("char", 1) }, 
	{ "int8", PlyPropertyType("char", 1) },
	{ "uchar", PlyPropertyType("uchar", 1) },
	{ "uint8", PlyPropertyType("uchar", 1) },
	{ "short", PlyPropertyType("short", 2) },
	{ "int16", PlyPropertyType("short", 2) },
	{ "ushort", PlyPropertyType("ushort", 2) },
	{ "uint16", PlyPropertyType("ushort", 2) },
	{ "int", PlyPropertyType("int", 4) },
	{ "int32", PlyPropertyType("int", 4) },
	{ "uint", PlyPropertyType("uint", 4) },
	{ "uint32", PlyPropertyType("uint", 4) },
	{ "float", PlyPropertyType("float", 4) },
	{ "float32", PlyPropertyType("float", 4) },
	{ "double", PlyPropertyType("double", 8) },
	{ "float64", PlyPropertyType("double", 8) }
};

vector<string> plyRedNames = { "r", "red", "diffuse_red" };
vector<string> plyGreenNames = { "g", "green", "diffuse_green" };
vector<string> plyBlueNames = { "b", "blue", "diffuse_blue" };

class PlyPointReader : public PointReader{
private:
	AABB *aabb;
	ifstream stream;
	int format;
	long pointCount;
	long pointsRead;
	PlyElement vertexElement;
	char *buffer;
	int pointByteSize;
	Point point;
	string file;

public:
	PlyPointReader(string file)
	: stream(file, std::ios::in | std::ios::binary)
	,vertexElement("vertexElement"){
		format = -1;
		pointCount = 0;
		pointsRead = 0;
		pointByteSize = 0;
		buffer = new char[100];
		aabb = NULL;
		this->file = file;

		std::regex rEndHeader("^end_header.*");
		std::regex rFormat("^format (ascii|binary_little_endian).*");
		std::regex rElement("^element (\\w*) (\\d*)");
		std::regex rProperty("^property (char|int8|uchar|uint8|short|int16|ushort|uint16|int|int32|uint|uint32|float|float32|double|float64) (\\w*)");
		
		string line;
		while(std::getline(stream, line)){
			line = trim(line);

			std::cmatch sm;
			if(std::regex_match(line, rEndHeader)){
				// stop line parsing when end_header is encountered
				break;
			}else if(std::regex_match(line.c_str(), sm, rFormat)){
				// parse format
				string f = sm[1];
				if(f == "ascii"){
					format = PLY_FILE_FORMAT_ASCII;
				}else if(f == "binary_little_endian"){
					format = PLY_FILE_FORMAT_BINARY_LITTLE_ENDIAN;
				}
			}else if(std::regex_match(line.c_str(), sm, rElement)){
				// parse vertex element declaration
				string name = sm[1];
				long count = atol(string(sm[2]).c_str());

				if(name != "vertex"){
					continue;
				}
				pointCount = count;

				while(true){
					std::streamoff len = stream.tellg();
					getline(stream, line);
					line = trim(line);
					if(std::regex_match(line.c_str(), sm, rProperty)){
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
		static const float R[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0.00588235294117645f,0.02156862745098032f,0.03725490196078418f,0.05294117647058827f,0.06862745098039214f,0.084313725490196f,0.1000000000000001f,0.115686274509804f,0.1313725490196078f,0.1470588235294117f,0.1627450980392156f,0.1784313725490196f,0.1941176470588235f,0.2098039215686274f,0.2254901960784315f,0.2411764705882353f,0.2568627450980392f,0.2725490196078431f,0.2882352941176469f,0.303921568627451f,0.3196078431372549f,0.3352941176470587f,0.3509803921568628f,0.3666666666666667f,0.3823529411764706f,0.3980392156862744f,0.4137254901960783f,0.4294117647058824f,0.4450980392156862f,0.4607843137254901f,0.4764705882352942f,0.4921568627450981f,0.5078431372549019f,0.5235294117647058f,0.5392156862745097f,0.5549019607843135f,0.5705882352941174f,0.5862745098039217f,0.6019607843137256f,0.6176470588235294f,0.6333333333333333f,0.6490196078431372f,0.664705882352941f,0.6803921568627449f,0.6960784313725492f,0.7117647058823531f,0.7274509803921569f,0.7431372549019608f,0.7588235294117647f,0.7745098039215685f,0.7901960784313724f,0.8058823529411763f,0.8215686274509801f,0.8372549019607844f,0.8529411764705883f,0.8686274509803922f,0.884313725490196f,0.8999999999999999f,0.9156862745098038f,0.9313725490196076f,0.947058823529412f,0.9627450980392158f,0.9784313725490197f,0.9941176470588236f,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0.9862745098039216f,0.9705882352941178f,0.9549019607843139f,0.93921568627451f,0.9235294117647062f,0.9078431372549018f,0.892156862745098f,0.8764705882352941f,0.8607843137254902f,0.8450980392156864f,0.8294117647058825f,0.8137254901960786f,0.7980392156862743f,0.7823529411764705f,0.7666666666666666f,0.7509803921568627f,0.7352941176470589f,0.719607843137255f,0.7039215686274511f,0.6882352941176473f,0.6725490196078434f,0.6568627450980391f,0.6411764705882352f,0.6254901960784314f,0.6098039215686275f,0.5941176470588236f,0.5784313725490198f,0.5627450980392159f,0.5470588235294116f,0.5313725490196077f,0.5156862745098039f,0.5f};
	static const float G[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0.001960784313725483f,0.01764705882352935f,0.03333333333333333f,0.0490196078431373f,0.06470588235294117f,0.08039215686274503f,0.09607843137254901f,0.111764705882353f,0.1274509803921569f,0.1431372549019607f,0.1588235294117647f,0.1745098039215687f,0.1901960784313725f,0.2058823529411764f,0.2215686274509804f,0.2372549019607844f,0.2529411764705882f,0.2686274509803921f,0.2843137254901961f,0.3f,0.3156862745098039f,0.3313725490196078f,0.3470588235294118f,0.3627450980392157f,0.3784313725490196f,0.3941176470588235f,0.4098039215686274f,0.4254901960784314f,0.4411764705882353f,0.4568627450980391f,0.4725490196078431f,0.4882352941176471f,0.503921568627451f,0.5196078431372548f,0.5352941176470587f,0.5509803921568628f,0.5666666666666667f,0.5823529411764705f,0.5980392156862746f,0.6137254901960785f,0.6294117647058823f,0.6450980392156862f,0.6607843137254901f,0.6764705882352942f,0.692156862745098f,0.7078431372549019f,0.723529411764706f,0.7392156862745098f,0.7549019607843137f,0.7705882352941176f,0.7862745098039214f,0.8019607843137255f,0.8176470588235294f,0.8333333333333333f,0.8490196078431373f,0.8647058823529412f,0.8803921568627451f,0.8960784313725489f,0.9117647058823528f,0.9274509803921569f,0.9431372549019608f,0.9588235294117646f,0.9745098039215687f,0.9901960784313726f,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0.9901960784313726f,0.9745098039215687f,0.9588235294117649f,0.943137254901961f,0.9274509803921571f,0.9117647058823528f,0.8960784313725489f,0.8803921568627451f,0.8647058823529412f,0.8490196078431373f,0.8333333333333335f,0.8176470588235296f,0.8019607843137253f,0.7862745098039214f,0.7705882352941176f,0.7549019607843137f,0.7392156862745098f,0.723529411764706f,0.7078431372549021f,0.6921568627450982f,0.6764705882352944f,0.6607843137254901f,0.6450980392156862f,0.6294117647058823f,0.6137254901960785f,0.5980392156862746f,0.5823529411764707f,0.5666666666666669f,0.5509803921568626f,0.5352941176470587f,0.5196078431372548f,0.503921568627451f,0.4882352941176471f,0.4725490196078432f,0.4568627450980394f,0.4411764705882355f,0.4254901960784316f,0.4098039215686273f,0.3941176470588235f,0.3784313725490196f,0.3627450980392157f,0.3470588235294119f,0.331372549019608f,0.3156862745098041f,0.2999999999999998f,0.284313725490196f,0.2686274509803921f,0.2529411764705882f,0.2372549019607844f,0.2215686274509805f,0.2058823529411766f,0.1901960784313728f,0.1745098039215689f,0.1588235294117646f,0.1431372549019607f,0.1274509803921569f,0.111764705882353f,0.09607843137254912f,0.08039215686274526f,0.06470588235294139f,0.04901960784313708f,0.03333333333333321f,0.01764705882352935f,0.001960784313725483f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	static const float B[] = {0.5f,0.5156862745098039f,0.5313725490196078f,0.5470588235294118f,0.5627450980392157f,0.5784313725490196f,0.5941176470588235f,0.6098039215686275f,0.6254901960784314f,0.6411764705882352f,0.6568627450980392f,0.6725490196078432f,0.6882352941176471f,0.7039215686274509f,0.7196078431372549f,0.7352941176470589f,0.7509803921568627f,0.7666666666666666f,0.7823529411764706f,0.7980392156862746f,0.8137254901960784f,0.8294117647058823f,0.8450980392156863f,0.8607843137254902f,0.8764705882352941f,0.892156862745098f,0.907843137254902f,0.9235294117647059f,0.9392156862745098f,0.9549019607843137f,0.9705882352941176f,0.9862745098039216f,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0.9941176470588236f,0.9784313725490197f,0.9627450980392158f,0.9470588235294117f,0.9313725490196079f,0.915686274509804f,0.8999999999999999f,0.884313725490196f,0.8686274509803922f,0.8529411764705883f,0.8372549019607844f,0.8215686274509804f,0.8058823529411765f,0.7901960784313726f,0.7745098039215685f,0.7588235294117647f,0.7431372549019608f,0.7274509803921569f,0.7117647058823531f,0.696078431372549f,0.6803921568627451f,0.6647058823529413f,0.6490196078431372f,0.6333333333333333f,0.6176470588235294f,0.6019607843137256f,0.5862745098039217f,0.5705882352941176f,0.5549019607843138f,0.5392156862745099f,0.5235294117647058f,0.5078431372549019f,0.4921568627450981f,0.4764705882352942f,0.4607843137254903f,0.4450980392156865f,0.4294117647058826f,0.4137254901960783f,0.3980392156862744f,0.3823529411764706f,0.3666666666666667f,0.3509803921568628f,0.335294117647059f,0.3196078431372551f,0.3039215686274508f,0.2882352941176469f,0.2725490196078431f,0.2568627450980392f,0.2411764705882353f,0.2254901960784315f,0.2098039215686276f,0.1941176470588237f,0.1784313725490199f,0.1627450980392156f,0.1470588235294117f,0.1313725490196078f,0.115686274509804f,0.1000000000000001f,0.08431372549019622f,0.06862745098039236f,0.05294117647058805f,0.03725490196078418f,0.02156862745098032f,0.00588235294117645f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
														            
		if(pointsRead == pointCount){
			return false;
		}
		
		double x = 0;
		double y = 0;
		double z = 0;
		float dummy;
		float nx = 0;
		float ny = 0;
		float nz = 0;
		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;
		
		//add by siyuan
		unsigned char intensity = 0;
		double intensity_double;
		float intensity_float;
		//add end

		if(format == PLY_FILE_FORMAT_ASCII){
			string line;
			getline(stream, line);
			line = trim(line);

			//vector<string> tokens;
			//split(tokens, line, is_any_of("\t "));
			vector<string> tokens = split(line, {'\t', ' '});
			int i = 0;
			for(const auto &prop : vertexElement.properties){
				string token = tokens[i++];
				if(prop.name == "x" && prop.type.name == plyPropertyTypes["float"].name){
					x = stof(token);
				}else if(prop.name == "y" && prop.type.name == plyPropertyTypes["float"].name){
					y = stof(token);
				}else if(prop.name == "z" && prop.type.name == plyPropertyTypes["float"].name){
					z = stof(token);
				}else if(prop.name == "x" && prop.type.name == plyPropertyTypes["double"].name){
					x = stod(token);
				}else if(prop.name == "y" && prop.type.name == plyPropertyTypes["double"].name){
					y = stod(token);
				}else if(prop.name == "z" && prop.type.name == plyPropertyTypes["double"].name){
					z = stod(token);
				}
				//add by siyuan
				else if (prop.name == "intensity"){
					intensity = stoi(token); 
				}

				//add end
				
				else if(std::find(plyRedNames.begin(), plyRedNames.end(), prop.name) != plyRedNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					r = (unsigned char)stof(token);
				}else if(std::find(plyGreenNames.begin(), plyGreenNames.end(), prop.name) != plyGreenNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					g = (unsigned char)stof(token);
				}else if(std::find(plyBlueNames.begin(), plyBlueNames.end(), prop.name) != plyBlueNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					b = (unsigned char)stof(token);
				}else if(prop.name == "nx" && prop.type.name == plyPropertyTypes["float"].name){
					nx = stof(token);
				}else if(prop.name == "ny" && prop.type.name == plyPropertyTypes["float"].name){
					ny = stof(token);
				}else if(prop.name == "nz" && prop.type.name == plyPropertyTypes["float"].name){
					nz = stof(token);
				}
			}
		}else if(format == PLY_FILE_FORMAT_BINARY_LITTLE_ENDIAN){
			stream.read(buffer, pointByteSize);

			int offset = 0;
			for(const auto &prop : vertexElement.properties){
				if(prop.name == "x" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&dummy, (buffer+offset), prop.type.size);
					x=dummy;
				}else if(prop.name == "y" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&dummy, (buffer+offset), prop.type.size);
					y=dummy;
				}else if(prop.name == "z" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&dummy, (buffer+offset), prop.type.size);
					z=dummy;
				}else if(prop.name == "x" && prop.type.name == plyPropertyTypes["double"].name){
					memcpy(&x, (buffer+offset), prop.type.size);
				}else if(prop.name == "y" && prop.type.name == plyPropertyTypes["double"].name){
					memcpy(&y, (buffer+offset), prop.type.size);
				}else if(prop.name == "z" && prop.type.name == plyPropertyTypes["double"].name){
					memcpy(&z, (buffer+offset), prop.type.size);
				}
				//add by siyuan
				else if (prop.name == "intensity" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&intensity_float, (buffer+offset), prop.type.size);
					intensity = intensity_float;
				}
				else if (prop.name == "intensity" && prop.type.name == plyPropertyTypes["double"].name){
					memcpy(&intensity_double, (buffer+offset), prop.type.size);
					intensity = intensity_double;
				}

				else if(std::find(plyRedNames.begin(), plyRedNames.end(), prop.name) != plyRedNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					memcpy(&r, (buffer+offset), prop.type.size);
				}else if(std::find(plyGreenNames.begin(), plyGreenNames.end(), prop.name) != plyGreenNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					memcpy(&g, (buffer+offset), prop.type.size);
				}else if(std::find(plyBlueNames.begin(), plyBlueNames.end(), prop.name) != plyBlueNames.end() && prop.type.name == plyPropertyTypes["uchar"].name){
					memcpy(&b, (buffer+offset), prop.type.size);
				}else if(prop.name == "nx" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&nx, (buffer+offset), prop.type.size);
				}else if(prop.name == "ny" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&ny, (buffer+offset), prop.type.size);
				}else if(prop.name == "nz" && prop.type.name == plyPropertyTypes["float"].name){
					memcpy(&nz, (buffer+offset), prop.type.size);
				}
				

				offset += prop.type.size;
			}
			
		}

		//added by siyuan
		r = 255 * R[intensity];
		g = 255 * G[intensity];
		b = 255 * B[intensity];

		point = Point(x,y,z,r,g,b);
		point.normal.x = nx;
		point.normal.y = ny;
		point.normal.z = nz;
		//add by siyuan
		point.intensity = intensity;
		//add end
		pointsRead++;
		return true;
	}

	Point getPoint(){
		return point;
	}

	AABB getAABB(){
		if(aabb == NULL){

			aabb = new AABB();

			PlyPointReader *reader = new PlyPointReader(file);
			while(reader->readNextPoint()){
				Point p = reader->getPoint();
				aabb->update(p.position);
			}

			reader->close();
			delete reader;

		}

		return *aabb;
	}

	long long numPoints(){
		return pointCount;
	}

	void close(){
		stream.close();
	}


};

}

#endif
