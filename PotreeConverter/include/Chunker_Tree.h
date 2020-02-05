
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>
#include <functional>

#include "Points.h"
#include "Vector3.h"
#include "LASWriter.hpp"
#include "TaskPool.h"

using std::string;
namespace fs = std::filesystem;

struct ChunkNode {

	struct BoundingBox {
		Vector3<double> min;
		Vector3<double> max;
	};

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;

	uint64_t storeSize = 1'000'000;
	
	uint64_t totalPoints = 0;

	vector<Point> points;
	vector<ChunkNode*> children;

	vector<int> childBinSize;

	string name = "";

	ChunkNode(string name, Vector3<double> min, Vector3<double> max) {
		this->name = name;
		this->min = min;
		this->max = max;
		this->size = max - min;

		childBinSize.resize(8, 0);

		//points.reserve(1'000'000);
	}

	void add(Point& point) {

		totalPoints++;

		if (totalPoints <= storeSize) {

			if (points.size() == 0) {
				points.reserve(1'000'000);
			}
			points.push_back(point);

			{
				double nx = (point.x - min.x) / size.x;
				double ny = (point.y - min.y) / size.y;
				double nz = (point.z - min.z) / size.z;

				int index = 0;

				index = index | (nx > 0.5 ? 0b100 : 0b000);
				index = index | (ny > 0.5 ? 0b010 : 0b000);
				index = index | (nz > 0.5 ? 0b001 : 0b000);

				childBinSize[index]++;
			}

		}else if(totalPoints == storeSize + 1){
			split();

			addToChild(point);
		} else {
			addToChild(point);
		}
		
	}

	void addToChild(Point& point) {

		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		int index = 0;

		if (index == 2) {
			int a = 0;
		}

		if (nx < 0.0 || nx > 1.0) {
			int b = 0;
		}

		if (ny < 0.0 || ny > 1.0) {
			int b = 0;
		}

		if (nz < 0.0 || nz > 1.0) {
			int b = 0;
		}

		index = index | (nx > 0.5 ? 0b100 : 0b000);
		index = index | (ny > 0.5 ? 0b010 : 0b000);
		index = index | (nz > 0.5 ? 0b001 : 0b000);

		auto child = children[index];

		if (point.x < child->min.x || point.x > child->max.x) {
			int damn = 0;
		}

		children[index]->add(point);
	}

	void split() {

		for (int i = 0; i < 8; i++) {
			BoundingBox box = childBoundingBoxOf(i);
			string childName = this->name + to_string(i);
			ChunkNode* child = new ChunkNode(childName, box.min, box.max);

			//child->points.reserve(childBinSize[i]);

			children.push_back(child);
		}

		for (Point& point : points) {
			addToChild(point);
		}

		points = vector<Point>();

	}

	BoundingBox childBoundingBoxOf(int index) {
		BoundingBox box;
		Vector3<double> center = min + (size * 0.5);

		if ((index & 0b100) == 0) {
			box.min.x = min.x;
			box.max.x = center.x;
		} else {
			box.min.x = center.x;
			box.max.x = max.x;
		}

		if ((index & 0b010) == 0) {
			box.min.y = min.y;
			box.max.y = center.y;
		} else {
			box.min.y = center.y;
			box.max.y = max.y;
		}

		if ((index & 0b001) == 0) {
			box.min.z = min.z;
			box.max.z = center.z;
		} else {
			box.min.z = center.z;
			box.max.z = max.z;
		}

		return box;
	}
};

class Chunker {
public:

	vector<Points*> batchesToDo;
	int32_t gridSize = 1; 
	string path = "";

	ChunkNode* root = nullptr;

	Chunker(string path, Attributes attributes, Vector3<double> min, Vector3<double> max) {
		this->path = path;
		this->gridSize = gridSize;

		root = new ChunkNode("r", min, max);
	}

	void close() {

		vector<ChunkNode*> nodes;

		function<void(ChunkNode*)> traverse = [&traverse, this, &nodes](ChunkNode* node) {

			nodes.push_back(node);

			for (ChunkNode* child : node->children) {

				if (child->totalPoints == 0) {
					continue;
				}

				traverse(child);
			}
		};

		traverse(root);

		return;

		auto path = this->path;
		auto flushProcessor = [path](shared_ptr<ChunkNode> node) {
			//cout << node->name << ": " << node->totalPoints << ", " << node->points.size() << endl;

			string lasPath = path + "/" + node->name + ".las";
			LASHeader header;
			header.min = node->min;
			header.max = node->max;
			header.numPoints = node->points.size();
			header.scale = { 0.001, 0.001, 0.001 };

			writeLAS(lasPath, header, node->points);
		};

		int numFlushThreads = 12;
		TaskPool<ChunkNode> pool(numFlushThreads, flushProcessor);

		for (ChunkNode* node : nodes) {
			pool.addTask(shared_ptr<ChunkNode>(node));
		}

		pool.close();

	}

	void add(shared_ptr<Points> batch) {

		for (Point& point : batch->points) {
			root->add(point);
		}

	}

};