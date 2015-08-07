

#ifndef POTREE_WRITER_H
#define POTREE_WRITER_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>              
#include <condition_variable> 

#include "liblas/liblas.hpp"
#include "boost/filesystem.hpp"

#include "Point.h"
#include "AABB.h"
#include "Vector3.h"
#include "stuff.h"

namespace fs = boost::filesystem;

using std::fstream;
using std::string;
using std::vector;
using std::ios;
using std::stringstream;
using std::map;
using std::unordered_map;
using std::isunordered;
using std::queue;
using std::mutex;
using std::condition_variable;
using std::thread;


namespace Potree{

static const int cells = 128;
static const int lastCellIndex = cells - 1;
static const int cellsHalf = cells / 2;

//string outDir = "D:/temp/test/out";

class PotreeWriter;

class Node{

public:

	PotreeWriter *writer = NULL;

	vector<Point> store;
	
	unsigned int storeLimit = 5'000'000;
	AABB aabb;
	int index = -1;
	Node *parent = NULL;
	vector<Node*> children;
	unsigned int numPoints = 0;
	string file = "";

	Node(PotreeWriter *writer, AABB aabb){
		this->writer = writer;
		this->aabb = aabb;
	}

	Node(PotreeWriter *writer, Node *parent, AABB aabb, int index){
		this->writer = writer;
		this->aabb = aabb;
		this->index = index;
		this->parent = parent;
	}

	bool isInnerNode(){
		return children.size() > 0;
	}

	string name() const {
		if(parent == NULL){
			return "r";
		}else{
			return parent->name() + std::to_string(index);
		}
	}

	string filename() const;

	void updateFilename(){
		if(!file.empty() && file != filename()){
			fs::rename(file, filename());
			file = filename();
		}
	}

	void split(){
		children = vector<Node*>{8, NULL};
	
		for(Point &point : store){
			add(point);
		}
		
		store.clear();
		store = vector<Point>();
	}

	void add(Point &point){
		if(!isInnerNode()){
			store.push_back(point);

			if(store.size() > storeLimit){
				split();
			}
		}else{

			double cdx = cells / aabb.size.x;
			double cdy = cells / aabb.size.y;
			double cdz = cells / aabb.size.z;
		
			// transform coordinates to range [0, cells]
			double ifx = cdx * (point.position.x - aabb.min.x);
			double ify = cdy * (point.position.y - aabb.min.y);
			double ifz = cdz * (point.position.z - aabb.min.z);

			// clamp to [0, cells - 1]; faster than using std::min 
			int ix = ifx >= cells ? lastCellIndex : int(ifx);
			int iy = ify >= cells ? lastCellIndex : int(ify);
			int iz = ifz >= cells ? lastCellIndex : int(ifz);
			
			// 3d indices to 1d index
			int index = ix + iy * cells + iz * cells * cells;

			int cix = ix / cellsHalf;
			int ciy = iy / cellsHalf;
			int ciz = iz / cellsHalf;
			int ci = cix << 2 | ciy << 1 | ciz;
			
			Node *childNode = children[ci];
			if(childNode == NULL){
				// create a child node at index ci
				AABB cAABB = childAABB(aabb, ci);
				childNode = new Node(writer, this, cAABB, ci);
				children[ci] = childNode;
			}

			childNode->add(point);

		}

	}

	bool isLeaf(){
		for(Node *child : children){
			if(child != NULL){
				return false;
			}
		}

		return true;
	}

	void flush();

};

class PotreeWriter{

public:
	vector<Point> store;
	unsigned int storeLimit = 100'000;
	Node *root = NULL;
	AABB aabb;
	long long numPoints = 0;
	string targetDir = "";

	PotreeWriter(string targetDir){
		this->targetDir = targetDir;

		fs::remove_all(targetDir + "/data");
		fs::create_directories(targetDir + "/data");
	}

	/**
	 * double octree size(and create new levels on top) until the given point fits
	 *
	 */
	void expandOctree(Point& point){


		// expand tree
		while(!root->aabb.isInside(point)){
			cout << endl;
			cout << "== increasing octree size == " << endl;

			double octreeWidth = root->aabb.size.x;

			// point not inside octree bounds. increase bounds
			double dx = point.position.x - root->aabb.min.x;
			double dy = point.position.y - root->aabb.min.y;
			double dz = point.position.z - root->aabb.min.z;

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

			Node *newRoot = new Node(this, newRootAABB);
			Node *oldRoot = this->root;

			cout << "old: " << oldRoot->aabb << "; new: " << newRoot->aabb << "; newIndex: " << oldRootIndex << endl;

			oldRoot->index = oldRootIndex;
			oldRoot->parent = newRoot;
			newRoot->children = vector<Node*>{8, NULL};
			newRoot->children[oldRootIndex] = oldRoot;
			this->root = newRoot;

			cout << "== end ==" << endl << endl;
		}

		// rename files
		vector<Node*> nodesToRename;
		queue<Node*> nodes;
		nodes.push(root);
		while(!nodes.empty()){
			Node* node = nodes.front();
			nodes.pop();

			nodesToRename.push_back(node);

			for(Node *child : node->children){
				if(child != NULL){
					nodes.push(child);
				}
			}
		}
		std::sort(nodesToRename.begin(), nodesToRename.end(), [](const Node *a, const Node *b){
			return a->name().size() > b->name().size();
		});
		for(Node *node : nodesToRename){
			node->updateFilename();
		}

	}

	void processStore(){

		//cout << "process store" << endl;

		if(root == NULL){
			AABB rootAABB = aabb;
			rootAABB.makeCubic();
			root = new Node(this, rootAABB);
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
		aabb.update(p.position);
		numPoints++;

		if(store.size() > storeLimit){
			processStore();
		}
	};

	void flush(){
		processStore();


		// traverse tree
		vector<Node*> nodesToFlush;
		vector<Node*> nodesToUnload;
		{
			queue<Node*> st;
			st.push(root);
			while(!st.empty()){
				Node *node = st.front();
				st.pop();

				//if(node->needsFlush){
					nodesToFlush.push_back(node);
				//}

				for(Node *child : node->children){
					if(child != NULL){
						st.push(child);
					}
				}
			}
		}

		{ // flush nodes
			cout << "nodes to flush: " << nodesToFlush.size() << endl;

			for(Node *node : nodesToFlush){
				node->flush();
			}

			//int offset = 0;
			//int numFlushThreads = 5;
			//vector<thread> threads;
			//mutex mtxOffset;
			//for(int i = 0; i < numFlushThreads; i++){
			//	threads.emplace_back([&mtxOffset, &nodesToFlush, &offset]{

			//			while(true){
			//				std::unique_lock<mutex> lock(mtxOffset);
			//				if(offset >= nodesToFlush.size()){
			//					lock.unlock();
			//					return;
			//				}
			//				Node *node = nodesToFlush[offset];
			//				offset++;
			//				lock.unlock();

			//				node->flush();
			//			}

			//	});
			//}

			//for(int i = 0; i < threads.size(); i++){
			//	threads[i].join();
			//}
		}


		//static int flushCount = 0;
		//string flushDir = outDir + "/../intermediate/" + std::to_string(flushCount);
		//copyDir(fs::path(outDir), flushDir);
		//flushCount++;
	}

};

}



#endif