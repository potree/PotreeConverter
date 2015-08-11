

#ifndef POTREEWRITER_H
#define POTREEWRITER_H

#include <string>
#include <sstream>
#include <list>
#include <thread>
#include <mutex>

#include "boost/filesystem.hpp"

#include "AABB.h"
#include "LASPointWriter.hpp"
#include "SparseGrid.h"
#include "stuff.h"
#include "CloudJS.hpp"
#include "PointReader.h"
#include "PointWriter.hpp"
#include "PointAttributes.hpp"

using std::string;
using std::stringstream;
using std::thread;
using std::mutex;
using std::lock_guard;

namespace fs = boost::filesystem;

namespace Potree{

class PotreeWriter;

class PotreeWriterNode{

public:
	string name;
	AABB aabb;
	AABB acceptedAABB;
	float spacing;
	int level;
	SparseGrid *grid;
	unsigned int numAccepted;
	PotreeWriterNode *parent;
	vector<PotreeWriterNode*> children;
	long long lastAccepted;
	bool addCalledSinceLastFlush;
	PotreeWriter *potreeWriter;
	vector<Point> cache;
	double scale;
	int storeLimit = 30'000;
	vector<Point> store;
	bool isInMemory = true;

	PotreeWriterNode(PotreeWriter* potreeWriter, string name, AABB aabb, float spacing, int level, double scale);

	~PotreeWriterNode(){
		for(PotreeWriterNode *child : children){
			if(child != NULL){
				delete child;
			}
		}
		delete grid;

		//delete [] children;
	}

	bool isLeafNode(){
		return children.size() == 0;
	}

	bool isInnerNode(){
		return children.size() > 0;
	}

	void addToDescendantsWithoutCheck(Point &point);

	void loadFromDisk();

	PotreeWriterNode *add(Point &point);

	PotreeWriterNode *createChild(int childIndex);

	void split();

	string workDir();

	string hierarchyPath();

	string path();

	void flush();

	void traverse(std::function<void(PotreeWriterNode*)> callback);

	vector<PotreeWriterNode*> getHierarchy(int levels);

private:

	PointReader *createReader(string path);
	PointWriter *createWriter(string path, double scale);

};



class PotreeWriter{

public:

	AABB aabb;
	AABB tightAABB;
	string workDir;
	float spacing;
	int maxDepth;
	PotreeWriterNode *root;
	long long numAccepted = 0;
	CloudJS cloudjs;
	OutputFormat outputFormat;
	PointAttributes pointAttributes;
	int hierarchyStepSize = 4;
	vector<Point> store;
	thread storeThread;
	int pointsInMemory = 0;



	PotreeWriter(string workDir, AABB aabb, float spacing, int maxDepth, double scale, OutputFormat outputFormat, PointAttributes pointAttributes){
		this->workDir = workDir;
		this->aabb = aabb;
		this->spacing = spacing;
		this->maxDepth = maxDepth;
		this->outputFormat = outputFormat;

		fs::create_directories(workDir + "/data");
		fs::create_directories(workDir + "/temp");

		this->pointAttributes = pointAttributes;

		cloudjs.outputFormat = outputFormat;
		cloudjs.boundingBox = aabb;
		cloudjs.octreeDir = "data";
		cloudjs.spacing = spacing;
		cloudjs.version = "1.7";
		cloudjs.scale = scale;
		cloudjs.pointAttributes = pointAttributes;

		root = new PotreeWriterNode(this, "r", aabb, spacing, 0, scale);
	}

	~PotreeWriter(){
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

	void processStore(){

		vector<Point> st = store;
		store = vector<Point>();

		if(storeThread.joinable()){
			storeThread.join();
		}

		storeThread = thread([this, st]{
			for(Point p : st){
				PotreeWriterNode *acceptedBy = root->add(p);
				if(acceptedBy != NULL){
					pointsInMemory++;
					numAccepted++;

					tightAABB.update(p.position);
				}
			}
		});
	}

	void add(Point &p){
		store.push_back(p);

		if(store.size() > 10'000){
			processStore();
		}
	}

	void flush(){
		processStore();

		if(storeThread.joinable()){
			storeThread.join();
		}

		root->flush();

		{// update cloud.js
			// long long numPointsInMemory = 0;  // unused variable
			// long long numPointsInHierarchy = 0  // unused variable;
			cloudjs.hierarchy = vector<CloudJS::Node>();
			cloudjs.hierarchyStepSize = hierarchyStepSize;
			cloudjs.tightBoundingBox = tightAABB;

			ofstream cloudOut(workDir + "/cloud.js", ios::out);
			cloudOut << cloudjs.getString();
			cloudOut.close();
		}
			

		{// write hierarchy
			list<PotreeWriterNode*> stack;
			stack.push_back(root);
			while(!stack.empty()){
				PotreeWriterNode *node = stack.front();
				stack.pop_front();
		
				//string dest = workDir + "/hierarchy/" + node->name + ".hrc";
				string dest = workDir + "/data/" + node->hierarchyPath() + "/" + node->name + ".hrc";
				ofstream fout;
				fout.open(dest, ios::out | ios::binary);
				vector<PotreeWriterNode*> hierarchy = node->getHierarchy(hierarchyStepSize + 1);
				for(const auto &descendant : hierarchy){
					if(descendant->level == node->level + hierarchyStepSize ){
						stack.push_back(descendant);
					}
		
					char children = 0;
					for(int j = 0; j < descendant->children.size(); j++){
						if(descendant->children[j] != NULL){
							children = children | (1 << j);
						}
					}
					//
					//
					//
					fout.write(reinterpret_cast<const char*>(&children), 1);
					fout.write(reinterpret_cast<const char*>(&(descendant->numAccepted)), 4);
					//fout.write("bla", 4);
		
				}
				fout.close();
		
			}
		
		
		
		}
	}

	void close(){
		flush();
	}

};

}

#endif
