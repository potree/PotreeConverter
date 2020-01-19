
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Vector3.h"
#include "Points.h"
#include "SparseGrid.h"
#include "stuff.h"

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
	int level = 0;
	int index = 0;

	Node* parent = nullptr;

	vector<shared_ptr<Node>> children = vector<shared_ptr<Node>>(8, nullptr);

	shared_ptr<SparseGrid> grid;
	
	vector<Point> store;
	int storeSize = 1'000;
	bool storeBroken = false;
	int storefreeLevels = 0;

	Node(string name, Vector3<double> min, Vector3<double> max, double spacing) {
		this->name = name;
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->spacing = spacing;
		this->level = name.size() - 1;

		grid = make_shared<SparseGrid>(min, max, spacing);
	}

	~Node() {
		
	}

	void setStorefreeLevels(int levels) {
		storefreeLevels = levels;
	}

	shared_ptr<Node> find(string id) {

		// TODO: this search function seems awful...

		if (this->name == id) {
			return shared_ptr<Node>(this);
		} else if (id.size() < this->name.size()) {
			return shared_ptr<Node>(nullptr);
		}

		string remainder = stringReplace(id, this->name, "");

		if (remainder.size() == id.size()) {
			return shared_ptr<Node>(nullptr);
		}

		for (auto child : children) {

			if (child == nullptr) {
				continue;
			}

			if (stringReplace(id, child->name, "").size() != id.size()) {

				if (child->name == id) {
					return child;
				} else {
					return child->find(id);
				}

			}

		}

		return shared_ptr<Node>(nullptr);
	}

	void remove(shared_ptr<Node> node) {
		
		int nodeIndex = -1;
		for (int i = 0; i < children.size(); i++) {

			auto child = children[i];

			if (child != nullptr && child->name == node->name) {
				nodeIndex = i;
			}
		}

		if (nodeIndex != -1) {
			//children.erase(children.begin() + nodeIndex);
			children[nodeIndex] = nullptr;
		}

	}

	void add(Point& candidate){

		bool hasStore = this->level > storefreeLevels;

		if (hasStore && !storeBroken && store.size() < storeSize) {
			store.push_back(candidate);
		} else if(hasStore && !storeBroken && store.size() >= storeSize) {
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

		//if (this->name == "r" && childIndex == 0) {

		//	childIndexOf(point);

		//	int a = 10;
		//}

		if (children[childIndex] == nullptr) {
			BoundingBox box = childBoundingBoxOf(point);

			string childName = this->name + to_string(childIndex);;
			Node* childRaw = new Node(childName, box.min, box.max, spacing * 0.5);
			childRaw->setStorefreeLevels(this->storefreeLevels);

			//shared_ptr<Node> child = make_shared<Node>(box.min, box.max, spacing * 0.5);
			shared_ptr<Node> child(childRaw);

			child->index = childIndex;
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