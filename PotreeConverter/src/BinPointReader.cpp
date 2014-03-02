

#include "BinPointReader.h"
#include "stuff.h"

#include <fstream>
#include <iostream>


using std::ifstream;
using std::cout;
using std::endl;
using std::ios;

BinPointReader::BinPointReader(string file)
	:stream(file, ios::in | ios::binary)
	,file(file)
{
	buffer = new char[4*10*1000*1000*sizeof(float)];
	points = reinterpret_cast<float*>(buffer);
	cPoints = buffer;
	batchSize = 10*1000*1000;
	batchByteSize = 4*batchSize*sizeof(float);
	offset = 0;
	localOffset = 0;
	currentBatchPointCount = 0;
}

void BinPointReader::close(){
	stream.close();
	delete[] buffer;
}

long BinPointReader::numPoints(){
	return filesize(file) / 16;
}

bool BinPointReader::readNextPoint(){

	// read next batch
	if(currentBatchPointCount == 0 || currentBatchPointCount == localOffset){
		stream.read(buffer, batchByteSize);
		currentBatchPointCount = (long)(stream.gcount() / (4*sizeof(float)));

		if(currentBatchPointCount == 0){
			return false;
		}

		localOffset = 0;
	}else{
		localOffset++;
	}

	return true;
}

Point BinPointReader::getPoint(){
	float x = points[4*localOffset+0];
	float y = points[4*localOffset+1];
	float z = points[4*localOffset+2];
	char r = cPoints[16*localOffset+12];
	char g = cPoints[16*localOffset+13];
	char b = cPoints[16*localOffset+14];
	Point p(x,y,z, r, g, b);

	return p;
}

AABB BinPointReader::getAABB(){
	return readAABB(file);
}