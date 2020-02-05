
#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <iostream>

#include "Metadata.h"
#include "Points.h"
#include "Node.h"
#include "SparseGrid.h"
#include "stuff.h"

using std::atomic;
using std::future;
using std::vector;
using std::thread;
using std::mutex;
using std::unique_lock;
using std::lock_guard;
using std::string;
using std::fstream;
using std::cout;
using std::endl;

namespace fs = std::filesystem;

struct Chunk {

	string file = "";
	//Points* points = nullptr;
	string id = "";
	//int index = -1;
	//Vector3<int> index3D;
	Vector3<double> min = {Infinity, Infinity, Infinity};
	Vector3<double> max = {-Infinity, -Infinity, -Infinity };

	Chunk() {

	}
};

class ChunkLoader;

vector<shared_ptr<Chunk>> getListOfChunks(Metadata& metadata);

shared_ptr<Points> loadChunk(shared_ptr<Chunk> chunk, Attributes attributes);

//shared_ptr<Node> processChunk(shared_ptr<Chunk> chunk, shared_ptr<Points> points, double spacing);


struct ProcessResult {

	// upperLevels starts from global root
	// it extents to eclusive chunkRoot, but does not connect to it,
	// so that we can throw away chunkRoot at some point and let shared_ptr 
	// remove chunkRoot from memory, while keeping upperLevels for later
	shared_ptr<Node> upperLevels;
	shared_ptr<Points> upperLevelsData;

	shared_ptr<Node> chunkRoot;

};

ProcessResult processChunk(
	shared_ptr<Chunk> chunk,
	shared_ptr<Points> points,
	Vector3<double> min, Vector3<double> max,
	double spacing);

