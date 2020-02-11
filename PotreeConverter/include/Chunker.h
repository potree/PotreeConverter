
#pragma once

#include "Points.h"
#include "math.h"
#include "stuff.h"



struct ChunkPiece {
	int index = -1;
	string name = "";
	string path = "";
	shared_ptr<Points> points;

	ChunkPiece(int index, string name, string path, shared_ptr<Points> points) {
		this->index = index;
		this->name = name;
		this->path = path;
		this->points = points;
	}
};




class Chunker {
public:

	string path = "";
	Attributes attributes;

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	Vector3<double> cellsD;

	int gridSize = 0;


	Chunker(string path, Attributes attributes, Vector3<double> min, Vector3<double> max, int gridSize);

	void close();

	string getName(int index);

	void add(shared_ptr<Points> batch);

};



void doChunking(string pathIn, string pathOut);