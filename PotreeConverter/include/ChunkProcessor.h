
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

namespace fs = std::experimental::filesystem;

struct Chunk {

	string file = "";
	Points* points = nullptr;
	int index = -1;
	Vector3<int> index3D;
	Vector3<double> min = {Infinity, Infinity, Infinity};
	Vector3<double> max = {-Infinity, -Infinity, -Infinity };

	Chunk() {

	}
};

class ChunkLoader;

vector<Chunk*> getListOfChunks(Metadata& metadata);

void loadChunk(Chunk* chunk);

Node* processChunk(Chunk* chunk);

