

#ifndef POTREEWRITER_H
#define POTREEWRITER_H

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
#include "PointAttributes.hpp"

using std::string;
using std::stringstream;

namespace fs = boost::filesystem;

class PotreeWriter;

class PotreeWriterNode{

public:
	string name;
	AABB aabb;
	AABB acceptedAABB;
	float spacing;
	int level;
	int maxLevel;
	SparseGrid *grid;
	unsigned int numAccepted;
	PotreeWriterNode *children[8];
	long long lastAccepted;
	bool addCalledSinceLastFlush;
    bool needsHrcFlush;
	PotreeWriter *potreeWriter;
	vector<Point> cache;
	double scale;

	PotreeWriterNode(PotreeWriter* potreeWriter, string name, AABB aabb, float spacing, int level, int maxLevel, double scale);

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

//	PotreeWriterNode *add(Point &point, int minLevel);

	PotreeWriterNode *createChild(int childIndex);

	string workDir();

	string hierarchyPath();

	string path();

	void flush();

	vector<PotreeWriterNode*> getHierarchy(int levels);

private:

	PointReader *createReader(string path);
	PointWriter *createWriter(string path, double scale);

};



class PotreeWriter{
private:
    long flushCount = 0;
public:

	AABB aabb;
	AABB tightAABB;
	string workDir;
	float spacing;
	int maxLevel;
	PotreeWriterNode *root;
	long long numAccepted;
	CloudJS cloudjs;
	OutputFormat outputFormat;
	PointAttributes pointAttributes;
	int hierarchyStepSize;

	int pointsInMemory;
	int pointsInMemoryLimit;



	PotreeWriter(string workDir, AABB aabb, float spacing, int maxLevel, double scale, OutputFormat outputFormat, vector<string> outputAttributes){
		this->workDir = workDir;
		this->aabb = aabb;
		this->spacing = spacing;
		this->maxLevel = maxLevel;
		this->outputFormat = outputFormat;
		numAccepted = 0;
		pointsInMemory = 0;
		pointsInMemoryLimit = 1*1000*1000;


		// TODO calculate step size instead
		if(maxLevel <= 5){
			hierarchyStepSize = 6;
		}else if(maxLevel <= 7){
			hierarchyStepSize = 4;
		}else if(maxLevel <= 9){
			hierarchyStepSize = 5;
		}else if(maxLevel <= 11){
			hierarchyStepSize = 4;
		}else if(maxLevel <= 14){
			hierarchyStepSize = 5;
		}else if(maxLevel <= 17){
			hierarchyStepSize = 6;
		}else if(maxLevel <= 19){
			hierarchyStepSize = 5;
		}else if(maxLevel <= 23){
			hierarchyStepSize = 6;
		}else if(maxLevel <= 24){
			hierarchyStepSize = 5;
		}else if(maxLevel <= 27){
			hierarchyStepSize = 4;
		}else if(maxLevel <= 29){
			hierarchyStepSize = 5;
		}else if(maxLevel <= 31){
			hierarchyStepSize = 4;
		}else{
			// I don't think this will happen anytime soon. This level would provide insane precision.
			hierarchyStepSize = 5;
		}

		fs::create_directories(workDir + "/data");
		fs::create_directories(workDir + "/temp");

		pointAttributes.add(PointAttribute::POSITION_CARTESIAN);

		for(int i = 0; i < outputAttributes.size(); i++){
			string attribute = outputAttributes[i];

			if(attribute == "RGB"){
				pointAttributes.add(PointAttribute::COLOR_PACKED);
			}else if(attribute == "INTENSITY"){
				pointAttributes.add(PointAttribute::INTENSITY);
			}else if(attribute == "CLASSIFICATION"){
				pointAttributes.add(PointAttribute::CLASSIFICATION);
			}
		}

		cloudjs.outputFormat = outputFormat;
		cloudjs.boundingBox = aabb;
		cloudjs.octreeDir = "data";
		cloudjs.spacing = spacing;
		cloudjs.version = "1.6";
		cloudjs.scale = scale;
		cloudjs.pointAttributes = pointAttributes;

		root = new PotreeWriterNode(this, "r", aabb, spacing, 0, maxLevel, scale);
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

	void add(Point &p){
		PotreeWriterNode *acceptedBy = root->add(p);

		if(acceptedBy != NULL){
			pointsInMemory++;
			numAccepted++;

			Vector3<double> position = p.position();
			tightAABB.update(position);
		}
	}

	void flush(bool forceHrc = false){
        bool flushHrc = flushCount < 20 ? true:
            flushCount < 100 ? 0 == flushCount % 5:
            flushCount < 400 ? 0 == flushCount % 10:
            flushCount < 1000 ? 0 == flushCount % 20:
            0 == flushCount % 50;
        flushHrc |= forceHrc;

        {// update cloud.js
			long long numPointsInMemory = 0;
			long long numPointsInHierarchy = 0;

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
            ofstream fout;
			while(!stack.empty()){
				PotreeWriterNode *node = stack.front();
				stack.pop_front();
                bool flushThisNodeHrc = flushHrc && (node->addCalledSinceLastFlush || node->needsHrcFlush);
		
                if (flushThisNodeHrc) {
                    string dest = workDir + "/data/" + node->hierarchyPath() + node->name + ".hrc";
                    fout.open(dest, ios::out | ios::binary);
                }
				vector<PotreeWriterNode*> hierarchy = node->getHierarchy(hierarchyStepSize + 1);
				for(int i = 0; i <  hierarchy.size(); i++){
					PotreeWriterNode *descendant = hierarchy[i];
					if(descendant->level == node->level + hierarchyStepSize && (node->level + hierarchyStepSize) < maxLevel ){
                        descendant->needsHrcFlush = true;
						stack.push_back(descendant);
					}
		
					char children = 0;
					for(int j = 0; j < 8; j++){
						if(descendant->children[j] != NULL){
							children = children | (1 << j);
						}
					}
                    
                    if (flushThisNodeHrc) {
                        fout.write(reinterpret_cast<const char*>(&children), 1);
                        fout.write(reinterpret_cast<const char*>(&(descendant->numAccepted)), 4);
                    }
				}
                if (flushThisNodeHrc) {
                    fout.close();
                }
			}
		}
        
        root->flush();
        flushCount++;
	}

	void close(){
		flush(true);
	}

};

#endif
