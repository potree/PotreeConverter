
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "convmath.h"
#include "Points.h"

using std::function;
using std::string;
using std::vector;
using std::shared_ptr;

namespace bluenoise {

struct Chunk {
	Vector3<double> min;
	Vector3<double> max;

	string file;
	string id;
};

struct Chunks {
	vector<shared_ptr<Chunk>> list;
	Vector3<double> min;
	Vector3<double> max;

	Chunks(vector<shared_ptr<Chunk>> list, Vector3<double> min, Vector3<double> max) {
		this->list = list;
		this->min = min;
		this->max = max;
	}

};

struct Node {

	string name;
	Vector3<double> min;
	Vector3<double> max;

	vector<Point> points;
	vector<Point> store;
	int64_t numPoints = 0;

	int storeSize = 10'000;
	bool storeExceeded = false;

	bool isFlushed = false;
	bool isSubsampled = false;

	Node* parent = nullptr;
	shared_ptr<Node> children[8] = { 
		nullptr, nullptr, nullptr, nullptr, 
		nullptr, nullptr, nullptr, nullptr};

	Node(string name, Vector3<double> min, Vector3<double> max);

	void processStore();

	void add(Point& point);

	void traverse(function<void(Node*)> callback);

	void traverse_postorder(function<void(Node*)> callback);

	static shared_ptr<Node> find(shared_ptr<Node> start, string targetName);

	void clear();

	bool isLeaf();

};

struct IndexedChunk {
	shared_ptr<Chunk> chunk;
	shared_ptr<Node> node;
};


}