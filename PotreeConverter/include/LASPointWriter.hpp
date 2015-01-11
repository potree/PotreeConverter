
#ifndef LASPOINTWRITER_H
#define LASPOINTWRITER_H

#include <string>
#include <iostream>
#include <fstream>

#include "liblas/liblas.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

#include "AABB.h"
#include "PointWriter.hpp"

using std::string;
using std::fstream;
using std::ios;
using boost::algorithm::iends_with;
namespace fs = boost::filesystem;

class LASPointWriter : public PointWriter{

public:
	string file;
	int numPoints;
	AABB aabb;
	fstream *stream;
	liblas::Writer *writer;
	liblas::Header *header;

	LASPointWriter(string file, AABB aabb, double scale) {
		this->file = file;
		this->aabb = aabb;
		
		if (fs::exists(file)){

			{ // create header with liblas reader
				std::ifstream ifs;
				ifs.open(file, std::ios::in | std::ios::binary);
				liblas::Reader *r = new liblas::Reader(ifs);
				const liblas::Header h = r->GetHeader();
				numPoints= h.GetPointRecordsCount();
				header = new liblas::Header(h);
			}

			//{ // create header without liblas reader
			//	header = new liblas::Header();
			//	header->SetDataFormatId(liblas::ePointFormat2);
			//	header->SetMin(aabb.min.x, aabb.min.y, aabb.min.z);
			//	header->SetMax(aabb.max.x, aabb.max.y, aabb.max.z);
			//	header->SetScale(scale, scale, scale);
			//	header->SetPointRecordsCount(53);
			//
			//	fstream *s = new fstream(file, ios::binary | ios::in );
			//	s->seekp(107);
			//	s->read(reinterpret_cast<char*>(&numPoints), 4);
			//	s->close();
			//	delete s;
			//
			//	header->SetPointRecordsCount(numPoints);
			//}

			stream = new fstream(file, std::ios::out | std::ios::in | std::ios::binary | std::ios::ate);
			writer = new liblas::Writer(*stream, *header);
		
			//ifs.close();
			//delete r;
		
		} else {
			numPoints = 0;

			header = new liblas::Header();
			header->SetDataFormatId(liblas::ePointFormat2);
			header->SetMin(aabb.min.x, aabb.min.y, aabb.min.z);
			header->SetMax(aabb.max.x, aabb.max.y, aabb.max.z);
			header->SetScale(scale, scale, scale);
			header->SetPointRecordsCount(53);

			if (iends_with(file, ".laz")) {
				header->SetCompressed(true);
			} else {
				header->SetCompressed(false);
			}

			stream = new fstream(file, ios::out | ios::binary);
			writer = new liblas::Writer(*stream, *header);
		}

	}

	~LASPointWriter(){
		close();
	}

	void write(const Point &point);

	void close(){

		if(writer != NULL){
			// close stream
			delete writer;
			stream->close();
			delete stream;
			writer = NULL;
			stream = NULL;
		
			// update point count
			stream = new fstream(file, ios::out | ios::binary | ios::in );
			stream->seekp(107);
			stream->write(reinterpret_cast<const char*>(&numPoints), 4);
			stream->close();
			delete stream;
			stream = NULL;

		}

		if(header != NULL) {
			delete header;
			header = NULL;
		}

	}

};

#endif


