

#ifndef POTREEWRITER_H
#define POTREEWRITER_H

#include <string>
#include <sstream>
#include <list>

#include "laswriter.hpp"

#include "AABB.h"
#include "LASPointWriter.hpp"
#include "SparseGrid.h"
#include "stuff.h"
#include "CloudJS.hpp"

using std::string;
using std::stringstream;

class PotreeWriter;

class PotreeWriterNode{

public:
	string name;
	string path;
	AABB aabb;
	AABB acceptedAABB;
	float spacing;
	int level;
	int maxLevel;
	SparseGrid *grid;
	int numAccepted;
	PotreeWriterNode *children[8];
	long long lastAccepted;
	bool addCalledSinceLastFlush;
	PotreeWriter *potreeWriter;
	vector<Point> cache;


	PotreeWriterNode(PotreeWriter* potreeWriter, string name, string path, AABB aabb, float spacing, int level, int maxLevel);

	~PotreeWriterNode(){
		for(int i = 0; i < 8; i++){
			if(children[i] != NULL){
				delete children[i];
			}
		}
		delete grid;

		//delete [] children;
	}

	void addToDescendantsWithoutCheck(Point &point);

	void loadFromDisk();

	PotreeWriterNode *add(Point &point);

	PotreeWriterNode *add(Point &point, int minLevel);

	PotreeWriterNode *createChild(int childIndex);

	void flush();

};



class PotreeWriter{

public:

	AABB aabb;
	string path;
	float spacing;
	int maxLevel;
	PotreeWriterNode *root;
	long long numAccepted;
	CloudJS cloudjs;

	int pointsInMemory;
	int pointsInMemoryLimit;



	PotreeWriter(string path, AABB aabb, float spacing, int maxLevel){
		this->path = path;
		this->aabb = aabb;
		this->spacing = spacing;
		this->maxLevel = maxLevel;
		numAccepted = 0;
		pointsInMemory = 0;
		pointsInMemoryLimit = 1*1000*1000;

		root = new PotreeWriterNode(this, "r", path, aabb, spacing, 0, maxLevel);
	}

	~PotreeWriter(){
		close();

		delete root;
	}


	void add(Point &p){
		PotreeWriterNode *acceptedBy = root->add(p);

		if(acceptedBy != NULL){
			pointsInMemory++;
			numAccepted++;
		}
	}

	void flush(){
		root->flush();





		// update cloud.js
		cloudjs.boundingBox = aabb;
		cloudjs.octreeDir = "data";
		cloudjs.outputFormat = OutputFormat::LAS;
		cloudjs.spacing = spacing;
		cloudjs.version = "1.3";
		
		long long numPointsInMemory = 0;
		long long numPointsInHierarchy = 0;
		cloudjs.hierarchy = vector<CloudJS::Node>();
		list<PotreeWriterNode*> stack;
		stack.push_back(root);
		while(!stack.empty()){
			PotreeWriterNode *node = stack.front();
			stack.pop_front();
			cloudjs.hierarchy.push_back(CloudJS::Node(node->name, node->numAccepted));
			numPointsInHierarchy += node->numAccepted;
			numPointsInMemory += node->grid->numAccepted;

			for(int i = 0; i < 8; i++){
				if(node->children[i] != NULL){
					stack.push_back(node->children[i]);
				}
			}
		}

		ofstream cloudOut(path + "/cloud.js", ios::out);
		cloudOut << cloudjs.string();
		cloudOut.close();
	}

	void close(){
		flush();
	}

};

#endif