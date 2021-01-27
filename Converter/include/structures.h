
#pragma once


#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <mutex>

#include "Vector3.h"
#include "unsuck/unsuck.hpp"
#include "Attributes.h"

using std::vector;
using std::shared_ptr;
using std::function;
using std::mutex;

struct CumulativeColor {
	int64_t r = 0;
	int64_t g = 0;
	int64_t b = 0;
	int64_t w = 0;
};

struct Node {

	vector<shared_ptr<Node>> children;

	string name;
	shared_ptr<Buffer> points;
	vector<CumulativeColor> colors;
	Vector3 min;
	Vector3 max;

	int64_t indexStart = 0;

	int64_t byteOffset = 0;
	int64_t byteSize = 0;
	int64_t numPoints = 0;

	bool sampled = false;

	Node() {

	}

	Node(string name, Vector3 min, Vector3 max) {
		this->name = name;
		this->min = min;
		this->max = max;

		children.resize(8, nullptr);
	}

	int64_t level() {
		return name.size() - 1;
	}

	void addDescendant(shared_ptr<Node> descendant) {
		static mutex mtx;
		lock_guard<mutex> lock(mtx);

		int descendantLevel = descendant->name.size() - 1;

		Node* current = this;

		for (int level = 1; level < descendantLevel; level++) {
			int index = descendant->name[level] - '0';

			if (current->children[index] != nullptr) {
				current = current->children[index].get();
			} else {
				string childName = current->name + to_string(index);
				auto box = childBoundingBoxOf(current->min, current->max, index);

				auto child = make_shared<Node>(childName, box.min, box.max);

				current->children[index] = child;

				current = child.get();
			}
		}

		auto index = descendant->name[descendantLevel] - '0';
		current->children[index] = descendant;
	}

	void traverse(function<void(Node*)> callback) {
		callback(this);

		for (auto child : children) {

			if (child != nullptr) {
				child->traverse(callback);
			}

		}
	}

	void traversePost(function<void(Node*)> callback) {
		for (auto child : children) {

			if (child != nullptr) {
				child->traversePost(callback);
			}
		}

		callback(this);
	}

	bool isLeaf() {

		for (auto child : children) {
			if (child != nullptr) {
				return false;
			}
		}


		return true;
	}
};

struct SamplerState {
	int bytesPerPoint;
	double baseSpacing;
	Vector3 scale;
	Vector3 offset;

	function<void(Node*)> writeAndUnload;
};


struct Sampler {


	Sampler() {

	}

	virtual void sample(shared_ptr<Node> node, Attributes attributes, double baseSpacing, function<void(Node*)> callbackNodeCompleted) = 0;

};