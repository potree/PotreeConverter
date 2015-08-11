

#include <cmath>

#include <stack>
#include <boost/filesystem.hpp>

#include "PotreeWriter.h"
#include "LASPointReader.h"
#include "BINPointReader.hpp"
#include "LASPointWriter.hpp"
#include "BINPointWriter.hpp"

using std::stack;

namespace fs = boost::filesystem;


PotreeWriterNode::PotreeWriterNode(PotreeWriter* potreeWriter, string name, AABB aabb, float spacing, int level, double scale){
	this->name = name;
	this->aabb = aabb;
	this->spacing = spacing;
	this->level = level;
	this->potreeWriter = potreeWriter;
	this->grid = new SparseGrid(aabb, spacing);
	this->scale = scale;
	numAccepted = 0;
	lastAccepted = 0;
	addCalledSinceLastFlush = false;
}

string PotreeWriterNode::workDir(){
	return potreeWriter->workDir;
}

string PotreeWriterNode::hierarchyPath(){
	string path = "r/";

	int hierarchyStepSize = potreeWriter->hierarchyStepSize;
	string indices = name.substr(1);

	int numParts = (int)floor((float)indices.size() / (float)hierarchyStepSize);
	for(int i = 0; i < numParts; i++){
		path += indices.substr(i * hierarchyStepSize, hierarchyStepSize) + "/";
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

		if(isLeafNode()){
			store.push_back(p);		
		}else{
			Vector3<double> position = Vector3<double>(p.x, p.y, p.z);
			grid->addWithoutCheck(position);
		}
	}
	grid->numAccepted = numAccepted;
	reader->close();
	delete reader;

	isInMemory = true;
}

PotreeWriterNode *PotreeWriterNode::createChild(int childIndex ){
	stringstream childName;
	childName << name << childIndex;
	AABB cAABB = childAABB(aabb, childIndex);
	PotreeWriterNode *child = new PotreeWriterNode(potreeWriter, childName.str(), cAABB, spacing / 2.0f, level+1, scale);
	child->parent = this;
	children[childIndex] = child;

	return child;
}

void PotreeWriterNode ::split(){
	children.resize(8, NULL);

	for(Point &point : store){
		add(point);
	}

	store = vector<Point>();
}

PotreeWriterNode *PotreeWriterNode::add(Point &point){
	addCalledSinceLastFlush = true;

	if(!isInMemory){
		loadFromDisk();
	}

	if(isLeafNode()){
		store.push_back(point);
		if(store.size() >= storeLimit){
			split();
		}

		return this;
	}else{
		Vector3<double> position(point.x, point.y, point.z);
		bool accepted = grid->add(position);

		//if(accepted){
		//	PotreeWriterNode *node = this->parent;
		//	while(accepted && node != NULL){
		//		accepted = accepted && node->grid->willBeAccepted(position);
		//
		//		node = node->parent;
		//	}
		//}

		if(accepted){
			cache.push_back(point);
			Vector3<double> position(point.x, point.y, point.z);
			acceptedAABB.update(position);
			numAccepted++;

			return this;
		}else{
			// try adding point to higher level

			if(potreeWriter->maxDepth != -1 && level >= potreeWriter->maxDepth){
				return NULL;
			}

			int childIndex = nodeIndex(aabb, point);
			if(childIndex >= 0){
				if(isLeafNode()){
					children.resize(8, NULL);
				}
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
}

void PotreeWriterNode::flush(){

	std::function<void(vector<Point> &points, bool append)> writeToDisk = [&](vector<Point> &points, bool append){
		string filepath = workDir() + "/data/" + path();
		PointWriter *writer = NULL;

		if(!fs::exists(workDir() + "/data/" + hierarchyPath())){
			fs::create_directories(workDir() + "/data/" + hierarchyPath());
		}

		if(append){
			string temppath = workDir() + "/temp/prepend" + potreeWriter->getExtension();
			if(fs::exists(filepath)){
				fs::rename(fs::path(filepath), fs::path(temppath));
			}

			writer = createWriter(filepath, scale);
			if(fs::exists(temppath)){
				PointReader *reader = createReader(temppath);
				while(reader->readNextPoint()){
					writer->write(reader->getPoint());
				}
				reader->close();
				delete reader;
				fs::remove(temppath);
			}
		}else{
			fs::remove(filepath);
			writer = createWriter(filepath, scale);
		}

		for(const auto &e_c : points){
			writer->write(e_c);
		}
		writer->close();
		delete writer;
	};


	if(isLeafNode()){
		if(addCalledSinceLastFlush){
			writeToDisk(store, false);		
		}else if(!addCalledSinceLastFlush && isInMemory){
			store = vector<Point>();
			isInMemory = false;
		}
	}else{
		if(addCalledSinceLastFlush){
			writeToDisk(cache, true);
			cache = vector<Point>();
		}else if(!addCalledSinceLastFlush && isInMemory){
			delete grid;
			grid = new SparseGrid(aabb, spacing);
			isInMemory = false;
		}
	}

	addCalledSinceLastFlush = false;

	for(PotreeWriterNode *child : children){
		if(child != NULL){
			child->flush();
		}
	}

	//if(cache.size() > 0){
	//	 // move data file aside to temporary directory for reading
	//	
	//
	//	cache = vector<Point>();
	////}else if(cache.size() == 0 && grid->numAccepted > 0 && addCalledSinceLastFlush == false){
	//}else if(isInMemory && grid->size() > 0 && !addCalledSinceLastFlush){
	//	delete grid;
	//	grid = new SparseGrid(aabb, spacing);
	//
	//	isInMemory = false;
	//}
	//
	//addCalledSinceLastFlush = false;
	//
	//for(PotreeWriterNode *child : children){
	//	if(child != NULL){
	//		child->flush();
	//	}
	//}
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
	
		for(PotreeWriterNode *child : node->children){
			if(child != NULL){
				stack.push_back(child);
			}
		}
	}

	return hierarchy;
}


void PotreeWriterNode::traverse(std::function<void(PotreeWriterNode*)> callback){

	stack<PotreeWriterNode*> st;
	st.push(this);
	while(!st.empty()){
		PotreeWriterNode *node = st.top();
		st.pop();

		callback(node);

		for(PotreeWriterNode *child : node->children){
			if(child != NULL){
				st.push(child);
			}
		}
	}
}

