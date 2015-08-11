

#ifndef POTREEWRITER_H
#define POTREEWRITER_H

#include <string>
#include <thread>

using std::string;
using std::thread;

namespace Potree{

class PotreeWriter;
class PointReader;
class PointWriter;

class PWNode{

public:
	string name;
	AABB aabb;
	AABB acceptedAABB;
	float spacing;
	int level;
	SparseGrid *grid;
	unsigned int numAccepted;
	PWNode *parent;
	vector<PWNode*> children;
	bool addCalledSinceLastFlush;
	PotreeWriter *potreeWriter;
	vector<Point> cache;
	double scale;
	int storeLimit = 30'000;
	vector<Point> store;
	bool isInMemory = true;

	PWNode(PotreeWriter* potreeWriter, string name, AABB aabb, float spacing, int level, double scale);

	~PWNode();

	bool isLeafNode(){
		return children.size() == 0;
	}

	bool isInnerNode(){
		return children.size() > 0;
	}

	void loadFromDisk();

	PWNode *add(Point &point);

	PWNode *createChild(int childIndex);

	void split();

	string workDir();

	string hierarchyPath();

	string path();

	void flush();

	void traverse(std::function<void(PWNode*)> callback);

	vector<PWNode*> getHierarchy(int levels);

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
	PWNode *root;
	long long numAccepted = 0;
	CloudJS cloudjs;
	OutputFormat outputFormat;
	PointAttributes pointAttributes;
	int hierarchyStepSize = 4;
	vector<Point> store;
	thread storeThread;
	int pointsInMemory = 0;



	PotreeWriter(string workDir, AABB aabb, float spacing, int maxDepth, double scale, OutputFormat outputFormat, PointAttributes pointAttributes);

	~PotreeWriter(){
		close();

		delete root;
	}

	string getExtension();

	void processStore();

	void waitUntilProcessed();

	void add(Point &p);

	void flush();

	void close(){
		flush();
	}

};

}

#endif
