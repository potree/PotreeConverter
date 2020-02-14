
#include "Indexer_Centered.h"

#include <vector>
#include <filesystem>
#include <memory>
#include <thread>
#include <unordered_map>

#include "json.hpp"

#include "convmath.h"
#include "converter_utils.h"
#include "stuff.h"
#include "LASWriter.hpp"
#include "TaskPool.h"
#include "LASLoader.hpp"


using std::vector;
using std::thread;
using std::unordered_map;
using std::string;
using std::to_string;
using std::make_shared;
using std::shared_ptr;
using json = nlohmann::json;

namespace fs = std::filesystem;

namespace centered_nochunks{

int gridSize = 128;
double dGridSize = gridSize;

struct Cell {
	Point point;
	double distance = Infinity;
	int childCellPointers[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
};

int numNodes = 0;
int numAdds = 0;
int maxDepth = 0;

struct Node{

	string name;
	int level = 0;
	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	vector<shared_ptr<Node>> children;
	vector<Cell> cells;


	Node(string name, Vector3<double> min, Vector3<double> max){
		this->name = name;
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->children.resize(8, nullptr);

		this->level = name.size() - 1;

		numNodes++;
	}

	int add(Point point, int cellPointer, int childIndex) {

		if (level >= 7) {
			return cellPointer;
		}

		numAdds++;
		maxDepth = std::max(maxDepth, level);

		double dix = dGridSize * (point.x - min.x) / size.x;
		double diy = dGridSize * (point.y - min.y) / size.y;
		double diz = dGridSize * (point.z - min.z) / size.z;

		int64_t ix = std::min(dix, dGridSize - 1.0);
		int64_t iy = std::min(diy, dGridSize - 1.0);
		int64_t iz = std::min(diz, dGridSize - 1.0);

		double dx = dix - double(ix) - 0.5;
		double dy = diy - double(iy) - 0.5;
		double dz = diz - double(iz) - 0.5;

		double distance = dx * dx + dy * dy + dz * dz;

		if (cellPointer == -1) {
			Cell cell;
			cell.point = point;
			cell.distance = distance;

			cells.push_back(cell);

			int newCellPointer = cells.size() - 1;

			return newCellPointer;
		} else {

			Cell& cell = cells[cellPointer];

			Point pointForChild;

			if (distance < cell.distance) {
				// new one is closer to center

				pointForChild = cell.point;

				cell.point = point;
				cell.distance = distance;
			}else{
				// old one is closer to center

				pointForChild = point;
			}

			
			auto child = children[childIndex];

			if (child == nullptr) {
				auto childBox = childBoundingBoxOf(min, max, childIndex);

				string childName = name + to_string(childIndex);
				child = make_shared<Node>(childName, childBox.min, childBox.max);
				children[childIndex] = child;
			}


			auto nextChildIndex = childIndexOf(child->min, child->max, pointForChild);

			int childCellIndex = 0;
			if (dx > 0.0) {
				childCellIndex = childCellIndex | 0b100;
			}
			if (dy > 0.0) {
				childCellIndex = childCellIndex | 0b010;
			}
			if (dz > 0.0) {
				childCellIndex = childCellIndex | 0b001;
			}

			auto childCellPointer = cell.childCellPointers[childCellIndex];




			int newChildPointer = child->add(pointForChild, childCellPointer, nextChildIndex);

			if (newChildPointer == 383) {
				int a = 10;
			}


			cell.childCellPointers[childCellIndex] = newChildPointer;

			int a = newChildPointer;
			int b = child->cells.size();
			if (a > b) {
				int a = 10;

				exit(1234);
			}

			return cellPointer;
			
		}

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

double addDuration = 0.0;

struct Indexer{

	Vector3<double> min;
	Vector3<double> max;

	//vector<Cell> cells;
	vector<int> cellPointers;
	shared_ptr<Node> root = nullptr;

	int pointsProcessed = 0;

	Indexer(Vector3<double> min, Vector3<double> max){
		this->min = min;
		this->max = max;

		cellPointers = vector<int>(gridSize * gridSize * gridSize, -1);

		root = make_shared<Node>("r", min, max);
	}

	

	void add(shared_ptr<Points> points){

		auto tStart = now();

		auto min = this->min;
		auto max = this->max;
		auto size = max - min;

		//auto indexOf = [min, max, size](Point& point) {
		//	uint64_t ix = std::min(dGridSize * (point.x - min.x) / size.x, dGridSize - 1.0);
		//	uint64_t iy = std::min(dGridSize * (point.y - min.y) / size.y, dGridSize - 1.0);
		//	uint64_t iz = std::min(dGridSize * (point.z - min.z) / size.z, dGridSize - 1.0);

		//	int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

		//	return index;
		//};

		//auto distanceOf = [min, max, size](Point& point) {
		//	double ix = std::min(dGridSize * (point.x - min.x) / size.x, dGridSize - 1.0);
		//	double iy = std::min(dGridSize * (point.y - min.y) / size.y, dGridSize - 1.0);
		//	double iz = std::min(dGridSize * (point.z - min.z) / size.z, dGridSize - 1.0);

		//	double dx = fmod(ix, 1.0) - 0.5;
		//	double dy = fmod(iy, 1.0) - 0.5;
		//	double dz = fmod(iz, 1.0) - 0.5;

		//	double d = dx * dx + dy * dy + dz * dz;

		//	return d;
		//};

		struct Result {
			int64_t index;
			double distance;
			int childIndex;
		};

		auto resultOf = [min, max, size](Point& point) -> Result{
			double dix = dGridSize * (point.x - min.x) / size.x;
			double diy = dGridSize * (point.y - min.y) / size.y;
			double diz = dGridSize * (point.z - min.z) / size.z;

			int64_t ix = std::min(dix, dGridSize - 1.0);
			int64_t iy = std::min(diy, dGridSize - 1.0);
			int64_t iz = std::min(diz, dGridSize - 1.0);

			double dx = dix - double(ix) - 0.5;
			double dy = diy - double(iy) - 0.5;
			double dz = diz - double(iz) - 0.5;

			int64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

			double distance = dx * dx + dy * dy + dz * dz;

			int childIndex = 0;
			if (ix > (gridSize / 2)) {
				childIndex = childIndex | 0b100;
			}
			if (iy > (gridSize / 2)) {
				childIndex = childIndex | 0b010;
			}
			if (iz > (gridSize / 2)) {
				childIndex = childIndex | 0b001;
			}

			return {index, distance, childIndex};
		};

		for (Point& point : points->points) {

			//auto index = indexOf(point);
			auto result = resultOf(point);
			auto index = result.index;
			auto childIndex = result.childIndex;

			auto childCellPointer = cellPointers[index];

			auto newChildCellPointer = root->add(point, childCellPointer, childIndex);

			if (childCellPointer == -1) {
				cellPointers[index] = newChildCellPointer;
			}

			pointsProcessed++;
		}

		auto duration = now() - tStart;
		addDuration += duration;

		cout << "#root.cells: " << root->cells.size() << endl;

		




	}



};

shared_ptr<Points> createTestData() {

	auto points = make_shared<Points>();

	//// center
	//points->points.emplace_back( 64.0,  64.0, 0.0, 0);

	//// near center
	//points->points.emplace_back(64.0 - 0.5, 64.0 - 0.5, 0.0, 0);
	//points->points.emplace_back(64.0 - 0.5, 64.0 + 0.5, 0.0, 0);
	//points->points.emplace_back(64.0 + 0.5, 64.0 - 0.5, 0.0, 0);
	//points->points.emplace_back(64.0 + 0.5, 64.0 + 0.5, 0.0, 0);


	//// corners
	//points->points.emplace_back(  0.0,   0.0, 0.0, 0); 
	//points->points.emplace_back(  0.0, 128.0, 0.0, 0); 
	//points->points.emplace_back(128.0,   0.0, 0.0, 0);
	//points->points.emplace_back(128.0, 128.0, 0.0, 0);

	int n = 2048;
	for (int i = 0; i < n; i++) {
	for (int j = 0; j < n; j++) {

		double x = 128.0 * double(i) / double(n - 1);
		double y = 128.0 * double(j) / double(n - 1);
		double z = 0;

		points->points.emplace_back(x, y, z, 0);

	}
	}



	return points;
}


void doIndexingTest() {
	auto data = createTestData();

	Vector3<double> min = { 0.0, 0.0, 0.0 };
	Vector3<double> max = { 128.0, 128.0, 128.0 };
	Indexer indexer(min, max);

	indexer.add(data);

	indexer.root->traverse([](Node* node) {

		vector<Point> accepted;
		for (auto cell : node->cells) {
			accepted.push_back(cell.point);
		}

		if (accepted.size() == 0) {
			return;
		}

		//accepted.push_back(Point(node->min.x, node->min.y, 0.0, 0));
		//accepted.push_back(Point(node->min.x, node->max.y, 0.0, 0));
		//accepted.push_back(Point(node->max.x, node->min.y, 0.0, 0));
		//accepted.push_back(Point(node->max.x, node->max.y, 0.0, 0));

		LASHeader header;
		header.min = node->min;
		header.max = node->max;
		header.numPoints = accepted.size();

		string filename = "D:/temp/test/" + node->name + ".las";

		writeLAS(filename, header, accepted);

	});

}


void doIndexing(string pathIn, string pathOut) {

	//doIndexingTest();

	//return;

	fs::create_directories(pathOut);

	LASLoader* loader = new LASLoader(pathIn, 1);
	Attributes attributes = loader->getAttributes();


	//for (const auto& entry : std::filesystem::directory_iterator(pathOut)) {
	//	std::filesystem::remove(entry);
	//}

	Vector3<double> size = loader->max - loader->min;
	double cubeSize = std::max(std::max(size.x, size.y), size.z);
	Vector3<double> cubeMin = loader->min;
	Vector3<double> cubeMax = cubeMin + cubeSize;

	Indexer indexer(cubeMin, cubeMax);

	double sum = 0;
	int batchNumber = 0;
	auto promise = loader->nextBatch();
	promise.wait();
	auto batch = promise.get();

	while (batch != nullptr) {
		if ((batchNumber % 10) == 0) {
			cout << "batch loaded: " << batchNumber << endl;
		}

		auto tStart = now();
		indexer.add(batch);
		auto duration = now() - tStart;
		sum += duration;

		promise = loader->nextBatch();
		promise.wait();
		batch = promise.get();

		batchNumber++;


		cout << "#nodes: " << numNodes << endl;
		cout << "#adds: " << numAdds << endl;
		cout << "#depth: " << maxDepth << endl;
	}

	cout << "indexing duration: " << sum << endl;
	cout << "raw add duration: " << addDuration << endl;


	


	indexer.root->traverse([](Node* node){

		vector<Point> accepted;
		for (auto cell : node->cells) {
			accepted.push_back(cell.point);
		}

		if (accepted.size() == 0) {
			return;
		}

		LASHeader header;
		header.min = node->min;
		header.max = node->max;
		header.numPoints = accepted.size();

		string filename = "D:/temp/test/" + node->name + ".las";

		writeLAS(filename, header, accepted);

	});


}

}