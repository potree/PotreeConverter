

#ifndef POTREE_WRITER_H
#define POTREE_WRITER_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <queue>
#include <thread>
#include <mutex>              
#include <condition_variable> 

#include "liblas/liblas.hpp"
#include "boost/filesystem.hpp"

#include "Point.h"
#include "AABB.h"
#include "Vector3.h"

namespace fs = boost::filesystem;

using std::fstream;
using std::string;
using std::vector;
using std::ios;
using std::stringstream;
using std::map;
using std::queue;
using std::mutex;
using std::condition_variable;
using std::thread;


namespace Potree{

static const int NODE_STORE_LIMIT = 10*1000;
static const int WRITER_STORE_LIMIT = 100*1000;
static const int cells = 64;
static const int lastCellIndex = cells - 1;
static const int cellsHalf = cells / 2;

string outDir = "D:/temp/test/out";


class Node{

public:

	vector<Point> store;
	vector<Point> selected;
	vector<int> grid;
	unsigned int storeLimit;
	AABB aabb;
	int index;
	Node *parent;
	vector<Node*> children;
	unsigned int numPoints;
	

	Node(AABB aabb){
		this->aabb = aabb;
		index = -1;
		storeLimit = 2*1000;
		children.resize(8, NULL);
		parent = NULL;
		numPoints = 0;
	}

	Node(Node *parent, AABB aabb, int index){
		this->aabb = aabb;
		this->index = index;
		this->parent = parent;
		storeLimit = 10*1000;
		children.resize(8, NULL);
		numPoints = 0;
	}

	string name(){
		if(parent == NULL){
			return "r";
		}else{
			return parent->name() + std::to_string(index);
		}
	}

	void processStore(){
		if(store.size() == 0){
			return;
		}

		if(grid.size() == 0){
			grid.resize(cells*cells*cells, -1);
		}

		double cdx = cells / aabb.size.x;
		double cdy = cells / aabb.size.y;
		double cdz = cells / aabb.size.z;

		for(Point &point : store){

			double ifx = cdx * (point.x - aabb.min.x);
			double ify = cdy * (point.y - aabb.min.y);
			double ifz = cdz * (point.z - aabb.min.z);

			//int ix = min(lastCellIndex, (int)ifx);		// 3.1%
			//int iy = min(lastCellIndex, (int)ify);		// 3.6%
			//int iz = min(lastCellIndex, (int)ifz);		// 2.2%

			// equivalent to previous commented lines but 2x faster according to VS2015 profiler
			int ix = ifx >= cells ? lastCellIndex : int(ifx);		// 1.7%
			int iy = ify >= cells ? lastCellIndex : int(ify);		// 1.4%
			int iz = ifz >= cells ? lastCellIndex : int(ifz);		// 1.4%
			

			int index = ix + iy * cells + iz * cells * cells;

			double dx = ifx - (ix + 0.5);
			double dy = ify - (iy + 0.5);
			double dz = ifz - (iz + 0.5);

			float distance = (float)(dx + dy + dz);
			point.distance = distance;
			bool accepted = distance < 0.2;
			accepted = true;

			int selectedIndex = grid[index];
			if(accepted && selectedIndex == -1){
				// add new point to grid
				selected.push_back(point);
				grid[index] = (int)selected.size() - 1;
				numPoints++;
			}else{
			
				//Point further = point;
				//
				//// replace point in grid
				//if(accepted && point.distance < selected[selectedIndex].distance){
				//	further = selected[selectedIndex];
				//	selected[selectedIndex] = point;
				//}
			
				Point *further;
				if(accepted && point.distance < selected[selectedIndex].distance) {
					further = &selected[selectedIndex];
					selected[selectedIndex] = point;
				}else{
					further = &point;
				}
			
			
				// pass down to next level
				//int cix = int(0.5 + (further.x - aabb.min.x) / aabb.size.x);
				//int ciy = int(0.5 + (further.y - aabb.min.y) / aabb.size.y);
				//int ciz = int(0.5 + (further.z - aabb.min.z) / aabb.size.z);
				int cix = ix / cellsHalf;
				int ciy = iy / cellsHalf;
				int ciz = iz / cellsHalf;
				int ci = cix << 2 | ciy << 1 | ciz;
			
			
			
				Node *childNode = children[ci];
				if(childNode == NULL){
					AABB cAABB = childAABB(aabb, ci);
					childNode = new Node(this, cAABB, ci);
					children[ci] = childNode;
				}
			
				childNode->add(*further);
			}
		}

		store.clear();
	}

	void add(Point &p){
		store.push_back(p);

		if(store.size() > storeLimit){
			processStore();
		}
	}

	bool needsFlush(){
		return selected.size() > 0 || store.size() > 0;
	}

	bool isLeaf(){
		for(Node *child : children){
			if(child != NULL){
				return false;
			}
		}

		return true;
	}

	void flush(){

		if(!needsFlush()){
			return;
		}

		//if(!isLeaf()){
		//	processStore();
		//}else{
			selected.insert(selected.end(), store.begin(), store.end());
		//}

		string file = outDir + "/" + name() + ".las";

		//cout << "flushing " << file << " points: " << this->selected.size() << endl;

		liblas::Header *header = new liblas::Header();
		header->SetDataFormatId(liblas::ePointFormat2);
		header->SetMin(this->aabb.min.x, this->aabb.min.y, this->aabb.min.z);
		header->SetMax(this->aabb.max.x, this->aabb.max.y, this->aabb.max.z);
		header->SetScale(0.001, 0.001, 0.001);
		header->SetPointRecordsCount(53);
	
		std::ofstream ofs;
		ofs.open(file, ios::out | ios::binary);
		liblas::Writer writer(ofs, *header);
	
		liblas::Point p(header);
		for(int i = 0; i < this->selected.size(); i++){
			Point &point = this->selected[i];
	
			p.SetX(point.x);
			p.SetY(point.y);
			p.SetZ(point.z);
	
			vector<uint8_t> &data = p.GetData();
	
			unsigned short pr = point.r * 256;
			unsigned short pg = point.g * 256;
			unsigned short pb = point.b * 256;
	
			liblas::Color color(int(point.r) * 256, int(point.g) * 256, int(point.b) * 256);
			p.SetColor(color);
	
			writer.WritePoint(p);
		} 
	
		ofs.close();
	
		delete header;
	
		// update point count
		int numPoints = (int)this->selected.size();
		std::fstream *stream = new std::fstream(file, ios::out | ios::binary | ios::in );
		stream->seekp(107);
		stream->write(reinterpret_cast<const char*>(&numPoints), 4);
		stream->close();
		delete stream;
		stream = NULL;

		selected.clear();
		grid.clear();
		store.clear();
	}

};

class PotreeWriter{

public:
	vector<Point> store;
	unsigned int storeLimit;
	Node *root;
	AABB aabb;
	long long numPoints;

	PotreeWriter(){
		root = NULL;
		storeLimit = 100*1000;
		numPoints = 0;
	}

	/**
	 * double octree size(and create new levels on top) until the given point fits
	 *
	 */
	void expandOctree(Point& point){

		while(!root->aabb.isInside(point)){
			cout << endl;
			cout << "== increasing octree size == " << endl;

			double octreeWidth = root->aabb.size.x;

			// point not inside octree bounds. increase bounds
			double dx = point.x - root->aabb.min.x;
			double dy = point.y - root->aabb.min.y;
			double dz = point.z - root->aabb.min.z;

			AABB newRootAABB(root->aabb);
			int oldRootIndex = 0;

			if(dx <= 0.0){
				newRootAABB.min.x -= octreeWidth;
				oldRootIndex = oldRootIndex | 4;
			}else{
				newRootAABB.max.x += octreeWidth;
			}

			if(dy <= 0.0){
				newRootAABB.min.y -= octreeWidth;
				oldRootIndex = oldRootIndex | 2;
			}else{
				newRootAABB.max.y += octreeWidth;
			}

			if(dz <= 0.0){
				newRootAABB.min.z -= octreeWidth;
				oldRootIndex = oldRootIndex | 1;
			}else{
				newRootAABB.max.z += octreeWidth;
			}
			newRootAABB.size = Vector3<double>(2*octreeWidth);

			Node *newRoot = new Node(newRootAABB);
			Node *oldRoot = this->root;

			cout << "old: " << oldRoot->aabb << "; new: " << newRoot->aabb << "; newIndex: " << oldRootIndex << endl;

			oldRoot->index = oldRootIndex;
			oldRoot->parent = newRoot;
			newRoot->children[oldRootIndex] = oldRoot;
			this->root = newRoot;

			newRoot->store.insert(newRoot->store.begin(), oldRoot->selected.begin(), oldRoot->selected.end());
			newRoot->store.insert(newRoot->store.begin(), oldRoot->store.begin(), oldRoot->store.end());
			oldRoot->selected.clear();
			oldRoot->store.clear();
			oldRoot->grid.clear();
			newRoot->processStore();

			cout << "== end ==" << endl << endl;
		}

	}

	void processStore(){

		//cout << "process store" << endl;

		if(root == NULL){
			AABB rootAABB = aabb;
			rootAABB.makeCubic();
			root = new Node(rootAABB);
		}

		for(Point &point : store){
			if(!root->aabb.isInside(point)){
				expandOctree(point);
			}

			root->add(point);
		}

		store.clear();

	}

	void add(Point &p){
		store.push_back(p);
		aabb.update(p.position());
		numPoints++;

		if(store.size() > storeLimit){
			processStore();
		}
	};

	void flush(){
		processStore();

		vector<Node*> nodesToFlush;
		queue<Node*> st;
		st.push(root);
		while(!st.empty()){
			Node *node = st.front();
			st.pop();

			nodesToFlush.push_back(node);

			if(!node->isLeaf()){
				node->processStore();
			}

			for(Node *child : node->children){
				if(child != NULL && child->needsFlush()){
					st.push(child);
				}
			}
		}

		cout << "nodes to flush: " << nodesToFlush.size() << endl;


		int offset = 0;
		int numFlushThreads = 5;
		vector<thread> threads;
		mutex mtxOffset;
		for(int i = 0; i < numFlushThreads; i++){
			threads.emplace_back([&mtxOffset, &nodesToFlush, &offset]{

					while(true){
						std::unique_lock<mutex> lock(mtxOffset);
						if(offset >= nodesToFlush.size()){
							lock.unlock();
							return;
						}
						Node *node = nodesToFlush[offset];
						offset++;
						lock.unlock();

						node->flush();
					}

			});
		}

		for(int i = 0; i < threads.size(); i++){
			threads[i].join();
		}

		//for(thread &tr : threads){
		//	tr.join();
		//}

		//or(Node *node : nodesToFlush){
		//	node->flush();
		//
	}

};

}



#endif