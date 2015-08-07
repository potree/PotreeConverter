

#ifndef POTREE_WRITER_H
#define POTREE_WRITER_H

#include <string>
#include <vector>

#include "Point.h"
#include "AABB.h"

using std::vector;
using std::string;

namespace Potree{

static const int cells = 128;
static const int lastCellIndex = cells - 1;
static const int cellsHalf = cells / 2;

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
	bool needsFlush = false;
	bool inMemory= true;

	Node(PotreeWriter *writer, AABB aabb);

	Node(PotreeWriter *writer, Node *parent, AABB aabb, int index);

	bool isInnerNode();

	string name() const;

	string filename() const;

	void updateFilename();

	void split();

	void add(Point &point);

	bool isLeaf();

	void flush();

	void unload();

	void traverse(std::function<void(Node*)> callback);

};

class PotreeWriter{

public:
	vector<Point> store;
	unsigned int storeLimit = 100'000;
	Node *root = NULL;
	AABB aabb;
	long long numPoints = 0;
	string targetDir = "";

	PotreeWriter(string targetDir);

	void expandOctree(Point& point);

	void processStore();

	void add(Point &p);

	void flush();

};

}



#endif