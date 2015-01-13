

#ifndef POTREEWRITER_DART_THROWING_H
#define POTREEWRITER_DART_THROWING_H

#include <string>
#include <sstream>
#include <list>

#include "boost/filesystem.hpp"

#include "AABB.h"
#include "LASPointWriter.hpp"
#include "SparseGrid.h"
#include "stuff.h"
#include "CloudJS.hpp"
#include "PointReader.h"
#include "PointWriter.hpp"
#include "PotreeWriter.h"

using std::string;
using std::stringstream;

namespace fs = boost::filesystem;

class PotreeWriter;
class PotreeWriterDartThrowing;

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
	PotreeWriterDartThrowing *potreeWriter;
	vector<Point> cache;
	double scale;

	PotreeWriterNode(PotreeWriterDartThrowing* potreeWriter, string name, string path, AABB aabb, float spacing, int level, int maxLevel, double scale);

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

private:

	PointReader *createReader(string path);
	PointWriter *createWriter(string path, double scale);

};



class PotreeWriterDartThrowing : public PotreeWriter{

public:

	AABB aabb;
	AABB tightAABB;
	string path;
	float spacing;
	int maxLevel;
	PotreeWriterNode *root;
	long long numAccepted;
	CloudJS cloudjs;
	OutputFormat outputFormat;

	int pointsInMemory;
	int pointsInMemoryLimit;
	int addedSinceLastFlush;



	PotreeWriterDartThrowing(string path, AABB aabb, float spacing, int maxLevel, double scale, OutputFormat outputFormat){
		this->path = path;
		this->aabb = aabb;
		this->spacing = spacing;
		this->maxLevel = maxLevel;
		this->outputFormat = outputFormat;
		numAccepted = 0;
		pointsInMemory = 0;
		pointsInMemoryLimit = 1*1000*1000;
		addedSinceLastFlush = 0;
		
		fs::create_directories(path + "/data");
		fs::create_directories(path + "/temp");
		

		cloudjs.outputFormat = outputFormat;
		cloudjs.boundingBox = aabb;
		cloudjs.octreeDir = "data";
		cloudjs.spacing = spacing;
		cloudjs.version = "1.4";
		cloudjs.scale = scale;

		root = new PotreeWriterNode(this, "r", path, aabb, spacing, 0, maxLevel, scale);
	}

	~PotreeWriterDartThrowing(){
		close();

		delete root;
	}

	string getExtension(){
		if(outputFormat == OutputFormat::LAS){
			return ".las";
		}else if(outputFormat == OutputFormat::LAZ){
			return ".laz";
		}else if(outputFormat == OutputFormat::BINARY){
			return ".bin";
		}

		return "";
	}

	void add(Point &p){
		PotreeWriterNode *acceptedBy = root->add(p);
		addedSinceLastFlush++;

		if(acceptedBy != NULL){
			pointsInMemory++;
			numAccepted++;

			Vector3<double> position = p.position();
			tightAABB.update(position);
		}

		if(addedSinceLastFlush >= 1*1000*1000){
			flush();
		}
	}

	void flush(){
		root->flush();





		// update cloud.js
		
		
		long long numPointsInMemory = 0;
		long long numPointsInHierarchy = 0;
		cloudjs.hierarchy = vector<CloudJS::Node>();
		cloudjs.tightBoundingBox = tightAABB;
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
		cloudOut << cloudjs.getString();
		cloudOut.close();

		addedSinceLastFlush = 0;
	}

	void close(){
		flush();
	}

	long long numPointsWritten(){
		return numAccepted;
	}

};

#endif
