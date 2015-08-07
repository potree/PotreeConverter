
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
#include <stack>

#include "liblas/liblas.hpp"
#include "boost/filesystem.hpp"

#include "Vector3.h"
#include "stuff.h"

#include "PotreeWriter.hpp"



namespace fs = boost::filesystem;

using std::fstream;
using std::string;
using std::vector;
using std::stack;
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

Node::Node(PotreeWriter *writer, AABB aabb){
	this->writer = writer;
	this->aabb = aabb;
}

Node::Node(PotreeWriter *writer, Node *parent, AABB aabb, int index){
	this->writer = writer;
	this->aabb = aabb;
	this->index = index;
	this->parent = parent;
}

bool Node::isInnerNode(){
	return children.size() > 0;
}

bool Node::isLeafNode(){
	return children.size() == 0;
}

string Node::name() const {
	if(parent == NULL){
		return "r";
	}else{
		return parent->name() + std::to_string(index);
	}
}

string Node::filename() const {
	return  this->writer->targetDir + "/data/" + name() + ".las";
}

void Node::updateFilename(){
	if(!file.empty() && file != filename()){
		fs::rename(file, filename());
		file = filename();
	}
}

void Node::split(){
	children = vector<Node*>{8, NULL};

	for(Point &point : store){
		add(point);
	}
	
	store.clear();
	store = vector<Point>();
}

void Node::loadFromDisk(){

	//cout << "loadFromDisk: " << filename() << endl;
	//
	//std::ifstream ifs;
	//ifs.open(filename(), std::ios::in | std::ios::binary);
	//liblas::ReaderFactory f;
	//liblas::Reader reader = f.CreateWithStream(ifs);
	//liblas::Header const& header = reader.GetHeader();
	//
	//while (reader.ReadNextPoint()){
	//	liblas::Point const& p = reader.GetPoint();
	//	liblas::Color color = p.GetColor();
	//
	//	unsigned char r = color.GetRed() / 256;
	//	unsigned char g = color.GetGreen() / 256;
	//	unsigned char b = color.GetBlue() / 256;
	//
	//	Point point(p.GetX(), p.GetY(), p.GetZ(), r, g, b);
	//	store.push_back(point);
	//}
	//
	//ifs.close();
	inMemory = true;
}

void Node::add(Point &point){


	if(isLeafNode()){
		if(!inMemory){
			loadFromDisk();
		}

		store.push_back(point);
		needsFlush = true;

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

void Node::unload(){
	cout << "unload: " << name() << endl;

	store = vector<Point>();

	inMemory = false;
}

void Node::flush(){

	string file = this->writer->targetDir + "/data/" + name() + ".las";

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

	for(int i = 0; i < this->store.size(); i++){
		Point &point = this->store[i];

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
	int numPoints = (int)this->store.size();
	std::fstream stream(file, ios::out | ios::binary | ios::in );
	stream.seekp(107);
	stream.write(reinterpret_cast<const char*>(&numPoints), 4);
	stream.close();

	this->file = file;

	needsFlush = false;
}

void Node::traverse(std::function<void(Node*)> callback){

	stack<Node*> st;
	st.push(this);
	while(!st.empty()){
		Node *node = st.top();
		st.pop();

		callback(node);

		for(Node *child : node->children){
			if(child != NULL){
				st.push(child);
			}
		}
	}
}


























PotreeWriter::PotreeWriter(string targetDir){
	this->targetDir = targetDir;

	fs::remove_all(targetDir + "/data");
	fs::create_directories(targetDir + "/data");
}

void PotreeWriter::expandOctree(Point& point){


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

	{// rename files
		vector<Node*> nodesToRename;
		root->traverse([&nodesToRename](Node *node){
			nodesToRename.push_back(node);
		});

		// rename from bottom to top to avoid overwriting nodes
		std::sort(nodesToRename.begin(), nodesToRename.end(), [](const Node *a, const Node *b){
			return a->name().size() > b->name().size();
		});

		for(Node *node : nodesToRename){
			node->updateFilename();
		}
	}

}

void PotreeWriter::flush(){
	processStore();

	root->traverse([](Node* node){
		if(!node->needsFlush && node->inMemory){
			node->unload();
		}else if(node->needsFlush){
			node->flush();
		}
	});

}

void PotreeWriter::processStore(){

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

void PotreeWriter::add(Point &p){
	store.push_back(p);
	aabb.update(p.position);
	numPoints++;

	if(store.size() > storeLimit){
		processStore();
	}
};


}

