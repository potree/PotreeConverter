
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

shared_ptr<Node> Node::find(shared_ptr<Node> root, string targetName){

	auto node = root;

	for(int i = 1; i < targetName.size(); i++){
		int childIndex = targetName.at(i) - '0';

		auto child = node->children[childIndex];

		if(child == nullptr){
			return nullptr;
		}else{
			node = child;
		}

	}

	if(node->name == targetName){
		return node;
	}else{
		cout << "ERROR: could not find node with name " << targetName << endl;

		exit(1234);

		return nullptr;
	}
}

void Node::clear(){
	//this->traverse([](Node* node){
		this->points.clear();
		this->store.clear();
		this->attributeBuffer = nullptr;
		
		this->points.shrink_to_fit();
		this->store.shrink_to_fit();
	//});
}

bool Node::isLeaf(){

	for(auto child : children){
		if(child != nullptr){
			return false;
		}
	}

	return true;
}

}