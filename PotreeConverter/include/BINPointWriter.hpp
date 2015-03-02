
#ifndef BINPOINTWRITER_H
#define BINPOINTWRITER_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "AABB.h"
#include "PointAttributes.hpp"
#include "PointWriter.hpp"

using std::string;
using std::vector;
using std::ofstream;
using std::ios;



class BINPointWriter : public PointWriter{

public:
	string file;
	int numPoints;
	PointAttributes attributes;
	ofstream *writer;
	AABB aabb;
	double scale;

	BINPointWriter(string file, AABB aabb, double scale) {
		this->file = file;
		this->aabb = aabb;
		this->scale = scale;
		numPoints = 0;
		attributes.add(PointAttribute::POSITION_CARTESIAN);
		attributes.add(PointAttribute::COLOR_PACKED);

		writer = new ofstream(file, ios::out | ios::binary);
	}

	BINPointWriter(string file, PointAttributes attributes) {
		this->file = file;
		numPoints = 0;
		this->attributes = attributes;

		writer = new ofstream(file, ios::out | ios::binary);
	}

	~BINPointWriter(){
		close();
	}

	void write(const Point &point){
		for(int i = 0; i < attributes.size(); i++){
			PointAttribute attribute = attributes[i];
			if(attribute == PointAttribute::POSITION_CARTESIAN){
				//float pos[3] = {(float) point.x,(float)  point.y,(float)  point.z};
				int x = (int)((point.x - aabb.min.x) / scale);
				int y = (int)((point.y - aabb.min.y) / scale);
				int z = (int)((point.z - aabb.min.z) / scale);
				int pos[3] = {x, y, z};
				writer->write((const char*)pos, 3*sizeof(int));
			}else if(attribute == PointAttribute::COLOR_PACKED){
                if (point.r == 0 && point.g == 0 && point.b == 0 && point.intensity != 0) {
                    unsigned char i = point.intensity >> 8;
                    unsigned char rgba[4] = {i, i, i, 255};
                    writer->write((const char*)rgba, 4*sizeof(unsigned char));
                } else {
				unsigned char rgba[4] = {point.r, point.g, point.b, 255};
				writer->write((const char*)rgba, 4*sizeof(unsigned char));
                }
			}
		}

		numPoints++;
	}

	void close(){
		if(writer != NULL){
			writer->close();
			delete writer;
			writer = NULL;
		}
	}

};

#endif


