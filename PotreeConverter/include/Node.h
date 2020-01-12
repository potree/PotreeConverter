
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Vector3.h"
#include "Points.h"
#include "SparseGrid.h"

class Node {

public:

	struct BoundingBox {
		Vector3<double> min;
		Vector3<double> max;
	};

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	double spacing = 1.0;

	string name = "";
	int index = 0;

	Node* parent = nullptr;
	vector<shared_ptr<Node>> children = vector<shared_ptr<Node>>(8, nullptr);

	shared_ptr<SparseGrid> grid;
	
	vector<Point> store;
	int storeSize = 1'000;
	bool storeBroken = false;

	Node(Vector3<double> min, Vector3<double> max, double spacing) {
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->spacing = spacing;

		grid = make_shared<SparseGrid>(min, max, spacing);
	}

	~Node() {
		
	}

	void add(Point& candidate){

		if (!storeBroken && store.size() < storeSize) {
			store.push_back(candidate);
		} else if(!storeBroken && store.size() >= storeSize) {
			breakStore();
		} else {
			auto action = grid->evaluate(candidate);

			bool keepGoing = 
				action.action == SparseGrid::Actions::exchange
				|| action.action == SparseGrid::Actions::pass;

			if (keepGoing) {
				addToChild(candidate);
			}
		}

	}

	void breakStore() {
		storeBroken = true;

		for (Point& point : store) {
			add(point);
		}

		store.clear();
		store.shrink_to_fit();
	}

	void addToChild(Point& point) {
		int childIndex = childIndexOf(point);

		if (children[childIndex] == nullptr) {
			BoundingBox box = childBoundingBoxOf(point);
			shared_ptr<Node> child = make_shared<Node>(box.min, box.max, spacing * 0.5);
			child->index = childIndex;
			child->name = this->name + to_string(childIndex);
			child->parent = this;

			children[childIndex] = child;
		}

		children[childIndex]->add(point);
	}

	int childIndexOf(Point& point) {

		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		int childIndex = 0;

		if (nx > 0.5) {
			childIndex = childIndex | 0b100;
		}

		if (ny > 0.5) {
			childIndex = childIndex | 0b010;
		}

		if (nz > 0.5) {
			childIndex = childIndex | 0b001;
		}

		return childIndex;
	}

	BoundingBox childBoundingBoxOf(Point& point) {
		BoundingBox box;
		Vector3<double> center = min + (size * 0.5);

		double nx = (point.x - min.x) / size.x;
		double ny = (point.y - min.y) / size.y;
		double nz = (point.z - min.z) / size.z;

		if (nx <= 0.5) {
			box.min.x = min.x;
			box.max.x = center.x;
		} else {
			box.min.x = center.x;
			box.max.x = max.x;
		}

		if (ny <= 0.5) {
			box.min.y = min.y;
			box.max.y = center.y;
		} else {
			box.min.y = center.y;
			box.max.y = max.y;
		}

		if (nz <= 0.5) {
			box.min.z = min.z;
			box.max.z = center.z;
		} else {
			box.min.z = center.z;
			box.max.z = max.z;
		}

		return box;
	}

	void traverse(function<void(Node*)> callback) {

		callback(this);

		for (auto child : children) {
			if (child == nullptr) {
				continue;
			}

			child->traverse(callback);
		}


	}

};