

#ifndef LASPOINTREADER_H
#define LASPOINTREADER_H

#include <string>
#include <iostream>
#include <vector>

#include <liblas/liblas.hpp>

#include "Point.h"
#include "PointReader.h"

using std::string;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;


class LIBLASReader{
public:

	ifstream stream;
	liblas::Reader reader;

	LIBLASReader(string path)
		: stream(path, std::ios::in | std::ios::binary)
		, reader(liblas::ReaderFactory().CreateWithStream(stream))
	{

	}

	~LIBLASReader(){
		if(stream.is_open()){
			stream.close();
		}
	}

	void close(){
		stream.close();
	}

};


class LASPointReader : public PointReader{
private:
	AABB aabb;
	string path;
	LIBLASReader *reader;
	vector<string> files;
	vector<string>::iterator currentFile;

public:

	LASPointReader(string path);

	~LASPointReader();

	bool readNextPoint();

	Point getPoint();

	AABB getAABB();

	long numPoints();

	void close();

	Vector3<double> getScale();
};

#endif