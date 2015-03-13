

#include <cmath>

#include <boost/filesystem.hpp>

#include "PotreeWriter.h"
#include "LASPointReader.h"
#include "BINPointReader.hpp"
#include "LASPointWriter.hpp"
#include "BINPointWriter.hpp"

namespace fs = boost::filesystem;


PotreeWriterNode::PotreeWriterNode(PotreeWriter* potreeWriter, string name, AABB aabb, float spacing, int level, int maxLevel, double scale){
	this->name = name;
	this->aabb = aabb;
	this->spacing = spacing;
	this->level = level;
	this->maxLevel = maxLevel;
	this->potreeWriter = potreeWriter;
	this->grid = new SparseGrid(aabb, spacing);
	this->scale = scale;
	numAccepted = 0;
	lastAccepted = 0;
	addCalledSinceLastFlush = false;

	for(int i = 0; i < 8; i++){
		children[i] = NULL;
	}
}

string PotreeWriterNode::workDir(){
	return potreeWriter->workDir;
}

string PotreeWriterNode::hierarchyPath(){
	string path = "";

	int hierachyStepSize = potreeWriter->hierarchyStepSize;
	string indices = name.substr(1);
	int numParts;
	if(indices.size() == 0){
		numParts = 0;
	}else{
		numParts = (int)ceil((float)indices.size() / (float)hierachyStepSize);
	}

	if(numParts == 0){
		path = "";
	}else{
		path = "";
		for(int i = 0; i < indices.length(); i++){
                        path += indices.substr(i,1)+ "/";
                }
	}

	return path;
}

string PotreeWriterNode::path(){
	string path = hierarchyPath() + "/" + name + potreeWriter->getExtension();
	return path;
}

PointReader *PotreeWriterNode::createReader(string path){
	PointReader *reader = NULL;
	OutputFormat outputFormat = this->potreeWriter->outputFormat;
	if(outputFormat == OutputFormat::LAS || outputFormat == OutputFormat::LAZ){
		reader = new LASPointReader(path);
	}else if(outputFormat == OutputFormat::BINARY){
		reader = new BINPointReader(path, aabb, scale, this->potreeWriter->pointAttributes);
	}

	return reader;
}

PointWriter *PotreeWriterNode::createWriter(string path, double scale){
	PointWriter *writer = NULL;
	OutputFormat outputFormat = this->potreeWriter->outputFormat;
	if(outputFormat == OutputFormat::LAS || outputFormat == OutputFormat::LAZ){
		writer = new LASPointWriter(path, aabb, scale);
	}else if(outputFormat == OutputFormat::BINARY){
		writer = new BINPointWriter(path, aabb, scale, this->potreeWriter->pointAttributes);
	}

	return writer;

	
}

void PotreeWriterNode::loadFromDisk(){
	PointReader *reader = createReader(workDir() + "/data/" + path());
	while(reader->readNextPoint()){
		Point p = reader->getPoint();
		Vector3<double> position = Vector3<double>(p.x, p.y, p.z);
		grid->addWithoutCheck(position);
	}
	grid->numAccepted = numAccepted;
	reader->close();
	delete reader;
}

PotreeWriterNode *PotreeWriterNode::add(Point &point, int minLevel){
	if(level > maxLevel){
		return NULL;
	}

	if(level < minLevel){
		// pass forth
		int childIndex = nodeIndex(aabb, point);
		if(childIndex >= 0){
			PotreeWriterNode *child = children[childIndex];

			if(child == NULL && level < maxLevel){
				child = createChild(childIndex);
			}

			return child->add(point, minLevel);
		}
	}else{
		return add(point);
	}
	
	return NULL;
}

PotreeWriterNode *PotreeWriterNode::createChild(int childIndex ){
	stringstream childName;
	childName << name << childIndex;
	AABB cAABB = childAABB(aabb, childIndex);
	PotreeWriterNode *child = new PotreeWriterNode(potreeWriter, childName.str(), cAABB, spacing / 2.0f, level+1, maxLevel, scale);
	children[childIndex] = child;

	return child;
}

PotreeWriterNode *PotreeWriterNode::add(Point &point){
	addCalledSinceLastFlush = true;

	// load grid from disk to memory, if necessary
	if(grid->numAccepted != numAccepted){
		loadFromDisk();
	}

	Vector3<double> position(point.x, point.y, point.z);
	bool accepted = grid->add(position);
	//float minGap = grid->add(Vector3<double>(point.x, point.y, point.z));
	//bool accepted = minGap > spacing;
	//int targetLevel = ceil(log((1/minGap) * spacing) / log(2));
	//
	//if(targetLevel > maxLevel){
	//	return NULL;
	//}

	if(accepted){
		cache.push_back(point);
		Vector3<double> position(point.x, point.y, point.z);
		acceptedAABB.update(position);
		numAccepted++;

		return this;
	}else if(level < maxLevel){
		// try adding point to higher level

		int childIndex = nodeIndex(aabb, point);
		if(childIndex >= 0){
			PotreeWriterNode *child = children[childIndex];

			// create child node if not existent
			if(child == NULL){
				child = createChild(childIndex);
			}

			return child->add(point);
			//child->add(point, targetLevel);
		} else {
			return NULL;
		}
	}
	return NULL;
}

void PotreeWriterNode::flush(){

	if(cache.size() > 0){
		 // move data file aside to temporary directory for reading
		string filepath = workDir() + "/data/" + path();
		string temppath = workDir() + "/temp/prepend" + potreeWriter->getExtension();
		if(fs::exists(filepath)){
			fs::rename(fs::path(filepath), fs::path(temppath));
		}

		if(!fs::exists(workDir() + "/data/" + hierarchyPath())){
			fs::create_directories(workDir() + "/data/" + hierarchyPath());
		}
		


		PointWriter *writer = createWriter(filepath, scale);
		if(fs::exists(temppath)){
			PointReader *reader = createReader(temppath);
			while(reader->readNextPoint()){
				writer->write(reader->getPoint());
			}
			reader->close();
			delete reader;
			fs::remove(temppath);
		}

		for(int i = 0; i < cache.size(); i++){
			writer->write(cache[i]);
		}
		writer->close();
		delete writer;

		cache = vector<Point>();
	}else if(cache.size() == 0 && grid->numAccepted > 0 && addCalledSinceLastFlush == false){
		delete grid;
		grid = new SparseGrid(aabb, spacing);
	}
	
	addCalledSinceLastFlush = false;

	for(int i = 0; i < 8; i++){
		if(children[i] != NULL){
			children[i]->flush();
		}
	}
}

vector<PotreeWriterNode*> PotreeWriterNode::getHierarchy(int levels){

	vector<PotreeWriterNode*> hierarchy;
	
	list<PotreeWriterNode*> stack;
	stack.push_back(this);
	while(!stack.empty()){
		PotreeWriterNode *node = stack.front();
		stack.pop_front();

		if(node->level >= this->level + levels){
			break;
		}
		hierarchy.push_back(node);

		for(int i = 0; i < 8; i++){
			if(node->children[i] != NULL){
				stack.push_back(node->children[i]);
			}
		}
	}

	return hierarchy;
}
