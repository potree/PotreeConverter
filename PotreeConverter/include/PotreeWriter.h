

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

using std::string;
using std::stringstream;

class PotreeWriterNode{

public:
	string name;
	string path;
	AABB aabb;
	AABB acceptedAABB;
	float spacing;
	int level;
	int maxLevel;
	LASheader lasHeader;
	LASPointWriter *writer;
	SparseGrid grid;
	int numAccepted;
	PotreeWriterNode *children[8];


	PotreeWriterNode(string name, string path, AABB aabb, float spacing, int level, int maxLevel)
		: grid(aabb, spacing)
	{
		this->name = name;
		this->path = path;
		this->aabb = aabb;
		this->spacing = spacing;
		this->level = level;
		this->maxLevel = maxLevel;
		this->writer = NULL;
		numAccepted = 0;

		lasHeader.clean();
		lasHeader.set_bounding_box(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
		lasHeader.x_scale_factor = 0.01;
		lasHeader.y_scale_factor = 0.01;
		lasHeader.z_scale_factor = 0.01;
		lasHeader.x_offset = 0.0f;
		lasHeader.y_offset = 0.0f;
		lasHeader.z_offset = 0.0f;

		for(int i = 0; i < 8; i++){
			children[i] = NULL;
		}
	}

	~PotreeWriterNode(){
		updateHeaders();
		close();
	}

	void close(){
		if(writer != NULL){
			writer->close();
			delete writer;
			writer = NULL;
		}

		grid = SparseGrid(aabb, spacing);

		for(int i = 0; i < 8; i++){
			if(children[i] != NULL){
				children[i]->close();
			}
		}
	}

	void updateHeaders(){
		if(writer != NULL){
			Vector3 min = acceptedAABB.min;
			Vector3 max = acceptedAABB.max;
			writer->header.set_bounding_box(min.x, min.y, min.z, max.x, max.y, max.z);
			writer->header.number_of_point_records = numAccepted;
			writer->header.start_of_waveform_data_packet_record = 0;
			writer->writer->update_header(&writer->header, false);
		}

		for(int i = 0; i < 8; i++){
			if(children[i] != NULL){
				children[i]->updateHeaders();
			}
		}
	}

	PotreeWriterNode *add(Point &point){
		bool accepted = grid.add(point);

		if(accepted){
			// write point to file

			if(writer == NULL){
				writer = new LASPointWriter(path + "/" + name + ".las", lasHeader);
			}

			writer->write(point);
			acceptedAABB.update(Vector3(point.x, point.y, point.z));
			numAccepted++;

			return this;
		}else if(level < maxLevel){
			// try adding point to higher level

			int childIndex = nodeIndex(aabb, point);
			if(childIndex >= 0){
				PotreeWriterNode *child = children[childIndex];

				// create child node if not existent
				if(child == NULL){
					stringstream childName;
					childName << name << childIndex;
					AABB cAABB = childAABB(aabb, childIndex);
					child = new PotreeWriterNode(childName.str(), path, cAABB, spacing / 2.0f, level+1, maxLevel);
					children[childIndex] = child;
				}

				child->add(point);
			}
		}else{
			return NULL;
		}
	}

};

class PotreeWriter{

public:

	AABB aabb;
	string path;
	float spacing;
	int maxLevel;
	PotreeWriterNode *root;

	int pointsInMemory;
	int pointsInMemoryLimit;



	PotreeWriter(string path, AABB aabb, float spacing, int maxLevel){
		this->path = path;
		this->aabb = aabb;
		this->spacing = spacing;
		this->maxLevel = maxLevel;
		pointsInMemory = 0;
		pointsInMemoryLimit = 1*1000*1000;

		root = new PotreeWriterNode("r", path, aabb, spacing, 0, maxLevel);
	}

	~PotreeWriter(){
		close();
		updateHeaders();

		delete root;
	}

	void add(Point &p){
		PotreeWriterNode *acceptedBy = root->add(p);
		pointsInMemory++;

		if(pointsInMemory > pointsInMemoryLimit){
			removeLeastRecentlyUsed();
		}
	}

	void removeLeastRecentlyUsed(){

	}

	void updateHeaders(){
		root->updateHeaders();
	};

	void close(){
		root->updateHeaders();
		root->close();
	}

};

#endif