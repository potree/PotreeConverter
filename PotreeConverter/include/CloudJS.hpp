
#ifndef CLOUDJS_H
#define CLOUDJS_H


#include <string>
#include <vector>
#include <sstream>
#include <list>

#include "AABB.h"
#include "definitions.hpp"

using std::string;
using std::vector;
using std::stringstream;
using std::list;

class Node{
public:
	string name;
	int pointCount;

	Node(string name, int pointCount){
		this->name = name;
		this->pointCount = pointCount;
	}
};

class CloudJS{
public:

	string version;
	string octreeDir;
	AABB boundingBox;
	OutputFormat outputFormat;
	double spacing;
	vector<Node> hierarchy;

	CloudJS(){
		version = "1.3";
	}

	string string(){
		stringstream cloudJs;

		cloudJs << "{" << endl;
		cloudJs << "\t" << "\"version\": \"" << version << "\"," << endl;
		cloudJs << "\t" << "\"octreeDir\": \"data\"," << endl;
		cloudJs << "\t" << "\"boundingBox\": {" << endl;
		cloudJs << "\t\t" << "\"lx\": " << boundingBox.min.x << "," << endl;
		cloudJs << "\t\t" << "\"ly\": " << boundingBox.min.y << "," << endl;
		cloudJs << "\t\t" << "\"lz\": " << boundingBox.min.z << "," << endl;
		cloudJs << "\t\t" << "\"ux\": " << boundingBox.max.x << "," << endl;
		cloudJs << "\t\t" << "\"uy\": " << boundingBox.max.y << "," << endl;
		cloudJs << "\t\t" << "\"uz\": " << boundingBox.max.z << endl;
		cloudJs << "\t" << "}," << endl;
		if(outputFormat == OutputFormat::BINARY){
			cloudJs << "\t" << "\"pointAttributes\": [" << endl;
			cloudJs << "\t\t" << "\"POSITION_CARTESIAN\"," << endl;
			cloudJs << "\t\t" << "\"COLOR_PACKED\"" << endl;
			cloudJs << "\t" << "]," << endl;
		}else if(outputFormat == OutputFormat::LAS){
			cloudJs << "\t" << "\"pointAttributes\": \"LAS\"," << endl;
		}else if(outputFormat == OutputFormat::LAZ){
			cloudJs << "\t" << "\"pointAttributes\": \"LAZ\"," << endl;
		}
		cloudJs << "\t" << "\"spacing\": " << spacing << "," << endl;
		cloudJs << "\t" << "\"hierarchy\": [" << endl;

		for(int i = 0; i < hierarchy.size(); i++){
			Node node = hierarchy[i];
			cloudJs << "\t\t[" << node.name << ", " << node.pointCount << "]";

			if(i < hierarchy.size()-1){
				cloudJs << ",";
			}
			cloudJs << endl;
		}
		cloudJs << "\t]" << endl;

		//list<Node*> stack;
		//stack.push_back(root);
		//while(!stack.empty){
		//	Node *current = stack.front;
		//	stack.pop_front();
		//
		//	cloudJs << "\t\t[" << current->name << ", " << current->pointCount << "]";
		//
		//	for(int i = 0; i < current->children.size(); i++){
		//		stack.push_back(current->children[i]);
		//	}
		//
		//	if(!stack.empty()){
		//		cloudJs << "," << endl;
		//	}
		//}
		//cloudJs << endl << "\t]";
		cloudJs << "}" << endl;

		return cloudJs.str();
	}
};


#endif