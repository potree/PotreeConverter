

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

string outDir = "D:/temp/test/out";


class Node{

public:

	vector<Point> store;
	vector<Point> selected;
	unordered_map<int, int> grid; 
	
	unsigned int storeLimit = 5'000;
	AABB aabb;
	int index = -1;
	Node *parent = NULL;
	vector<Node*> children{8, NULL};
	unsigned int numPoints = 0;
	bool needsFlush = false;
	string file = "";
	bool isInMemory = true;
	

	Node(AABB aabb){
		this->aabb = aabb;
	}

	Node(Node *parent, AABB aabb, int index){
		this->aabb = aabb;
		this->index = index;
		this->parent = parent;
	}

	string name() const {
		if(parent == NULL){
			return "r";
		}else{
			return parent->name() + std::to_string(index);
		}
	}

	string filename() const {
		return  outDir + "/" + name() + ".las";
	}

	void updateFilename(){
		if(!file.empty() && file != filename()){
			fs::rename(file, filename());
			file = filename();
		}
	}

	void loadFromDisk(){
		std::ifstream ifs;
		ifs.open(filename(), std::ios::in | std::ios::binary);
		liblas::ReaderFactory f;
		liblas::Reader reader = f.CreateWithStream(ifs);
		liblas::Header const& header = reader.GetHeader();

		while(reader.ReadNextPoint()){
			liblas::Point const& p = reader.GetPoint();
			liblas::Color color = p.GetColor();

			unsigned char r = color.GetRed();
			unsigned char g = color.GetGreen();
			unsigned char b = color.GetBlue();

			Point point(p.GetX(), p.GetY(), p.GetZ(), r, g, b);
			selected.push_back(point);
		}

		isInMemory = true;
	}

	void unload(){
		selected.clear();
		store.clear();
		// TODO: grid.clear();
		isInMemory = false;
	}

	void processStore(){
		if(store.size() == 0){
			return;
		}

		if(grid.size() == 0){
			grid.reserve(20'000);
		}

		if(!isInMemory){
			loadFromDisk();
		}


		double cdx = cells / aabb.size.x;
		double cdy = cells / aabb.size.y;
		double cdz = cells / aabb.size.z;

		for(Point &point : store){

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

			// distance to center
			double dx = ifx - (ix + 0.5);
			double dy = ify - (iy + 0.5);
			double dz = ifz - (iz + 0.5);
			float distance = (float)(dx + dy + dz);
			point.distance = distance;
			bool withinRange = distance < 0.3;
			withinRange = true;

			auto it = grid.find(index);

			bool cellIsEmpty = it == grid.end();
			if(withinRange && cellIsEmpty){
				// in range and cell is empty -> add to cell
				selected.push_back(point);
				grid.insert({index, (int)selected.size() - 1});
				numPoints++;
			}else{
				// create a child node at index ci
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

				if(withinRange && !cellIsEmpty && point.distance < selected[it->second].distance){
					// in range and cell is not empty but new point is closer than existing point
					// -> replace existing and pass existing down to next level
					Point *further = &selected[it->second];
					selected[it->second] = point;

					childNode->add(*further);
				}else{
					// not in range or point is farther away than existing point -> pass down to next level

					childNode->add(point);
				}
			}
		}

		store.clear();
	}

	void add(Point &p){
		store.push_back(p);
		needsFlush = true;

		if(store.size() > storeLimit){
			processStore();
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

	void flush(){
		if(!needsFlush){
			return;
		}

		selected.insert(selected.end(), store.begin(), store.end());

		string file = outDir + "/" + name() + ".las";

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
	
			p.SetX(point.position.x);
			p.SetY(point.position.y);
			p.SetZ(point.position.z);
	
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

		//clear();
		store.clear();

		this->file = file;
		needsFlush = false;
	}

};

class PotreeWriter{

public:
	vector<Point> store;
	unsigned int storeLimit = 100'000;
	Node *root = NULL;
	AABB aabb;
	long long numPoints = 0;

	PotreeWriter() = default;

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

				if(!node->isLeaf()){
					node->processStore();
				}

				if(node->needsFlush){
					nodesToFlush.push_back(node);
				}
				if(!node->needsFlush && node->isInMemory){
					nodesToUnload.push_back(node);
				}

				for(Node *child : node->children){
					if(child != NULL && child->needsFlush){
						st.push(child);
					}
				}
			}
		}

		{ // unload nodes
			for(Node *node : nodesToUnload){
				node->unload();
			}
		}

		{ // flush nodes
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
		}

		//static int flushCount = 0;
		//string flushDir = outDir + "/../intermediate/" + std::to_string(flushCount);
		//copyDir(fs::path(outDir), flushDir);
		//flushCount++;
	}

};

}



#endif