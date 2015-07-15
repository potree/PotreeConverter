

#ifndef POTREE_WRITER_H
#define POTREE_WRITER_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include "liblas/liblas.hpp"
#include "boost/filesystem.hpp"

#include "Point.h"
#include "AABB.h"
#include "Vector3.h"

namespace fs = boost::filesystem;

using std::fstream;
using std::string;
using std::vector;
using std::ios;


namespace Potree{

class PotreeWriter{
	string path;
	vector<Point> store;
	AABB aabb;
	double scale;

public:
	PotreeWriter(string path){
		this->path = path;
		scale = 0.001;

		fs::path p(path);
		fs::create_directories(p);
	}

	void add(Point &p){
		store.push_back(p);

		aabb.update(p.position());
	}

	void flush(){
		cout << "flushing" << endl;

		liblas::Header *header = new liblas::Header();
		header->SetDataFormatId(liblas::ePointFormat2);
		header->SetMin(aabb.min.x, aabb.min.y, aabb.min.z);
		header->SetMax(aabb.max.x, aabb.max.y, aabb.max.z);
		header->SetScale(scale, scale, scale);
		header->SetPointRecordsCount(53);

		string file = path + "/" + "test.las"; 
		cout << "file: " << file << endl;

		fstream *stream = new fstream(file, ios::out | ios::binary);
		liblas::Writer *writer = new liblas::Writer(*stream, *header);



		liblas::Point lp(header);
		for(Point &point : store){
	
			lp.SetX(point.x);
			lp.SetY(point.y);
			lp.SetZ(point.z);

			vector<uint8_t> &data = lp.GetData();

			unsigned short pr = point.r * 256;
			unsigned short pg = point.g * 256;
			unsigned short pb = point.b * 256;
	
			//liblas::Color color(int(point.r) * 256, int(point.g) * 256, int(point.b) * 256);
			//lp.SetColor(color);
			{ // TODO lp.SetColor did not work, do this instead. check for bugs?
				data[20] = reinterpret_cast<unsigned char*>(&pr)[0];
				data[21] = reinterpret_cast<unsigned char*>(&pr)[1];
	
				data[22] = reinterpret_cast<unsigned char*>(&pg)[0];
				data[23] = reinterpret_cast<unsigned char*>(&pg)[1];
	
				data[24] = reinterpret_cast<unsigned char*>(&pb)[0];
				data[25] = reinterpret_cast<unsigned char*>(&pb)[1];
			}

			lp.SetIntensity(point.intensity);
			lp.SetClassification(point.classification);
			lp.SetReturnNumber(point.returnNumber);
			lp.SetNumberOfReturns(point.numberOfReturns);
			lp.SetPointSourceID(point.pointSourceID);

			writer->WritePoint(lp);

		}


		stream->close();
		delete writer;
		delete stream;


		stream = new fstream(file, ios::out | ios::binary | ios::in );
		stream->seekp(107);
		int numPoints = store.size();
		stream->write(reinterpret_cast<const char*>(&numPoints), 4);
		stream->close();
		delete stream;
		stream = NULL;



		cout << "flushing done" << endl;





	}

};

}



#endif