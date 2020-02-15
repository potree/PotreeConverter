
#include "Structures.h"

#include "converter_utils.h"

#include <memory>

using std::to_string;
using std::make_shared;

namespace bluenoise{


Node::Node(string name, Vector3<double> min, Vector3<double> max) {
	this->name = name;
	this->min = min;
	this->max = max;

	store.reserve(10'000);
}

void Node::processStore() {
	storeExceeded = true;

	for (Point& point : store) {
		add(point);
	}

	store.clear();
	store.shrink_to_fit();
}

void Node::add(Point& point) {

	numPoints++;

	if (!storeExceeded) {
		store.push_back(point);

		if (store.size() == storeSize) {
			processStore();
		}

		return;
	}

	
	int childIndex = childIndexOf(min, max, point);

	auto child = children[childIndex];

	if (child == nullptr) {

		string childName = name + to_string(childIndex);
		auto box = childBoundingBoxOf(min, max, childIndex);

		if (childName.size() > 10) {
			int a = 10;
		}

		child = make_shared<Node>(childName, box.min, box.max);
		child->parent = this;
		children[childIndex] = child;
	}

	child->add(point);
}

void Node::traverse(function<void(Node*)> callback) {

	callback(this);

	for (auto child : children) {
		if (child == nullptr) {
			continue;
		}

		child->traverse(callback);
	}

}

void Node::traverse_postorder(function<void(Node*)> callback){
	for (auto child : children) {
		if (child == nullptr) {
			continue;
		}

		child->traverse_postorder(callback);
	}

	callback(this);
}

}