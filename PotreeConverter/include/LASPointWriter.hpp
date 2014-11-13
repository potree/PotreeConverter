
#ifndef LASPOINTWRITER_H
#define LASPOINTWRITER_H

#include <string>
#include <iostream>
#include <fstream>

#include "liblas/liblas.hpp"
#include <boost/algorithm/string/predicate.hpp>

#include "AABB.h"
#include "PointWriter.hpp"

using std::string;
using std::fstream;
using std::ios;
using boost::algorithm::iends_with;

class LASPointWriter : public PointWriter{

public:
	string file;
	int numPoints;
	AABB aabb;
	fstream *stream;
	liblas::Writer *writer;
	liblas::Header *header;

	LASPointWriter(string file, AABB aabb) {
		this->file = file;
		this->aabb = aabb;
		numPoints = 0;

		header = new liblas::Header();
		header->SetDataFormatId(liblas::ePointFormat2);
		header->SetMin(aabb.min.x, aabb.min.y, aabb.min.z);
		header->SetMax(aabb.max.x, aabb.max.y, aabb.max.z);
		header->SetScale(0.01, 0.01, 0.01);
		header->SetPointRecordsCount(53);

		if(iends_with(file, ".laz")){
			header->SetCompressed(true);
		}else{
			header->SetCompressed(false);
		}

		stream = new fstream(file, ios::out | ios::binary);
		writer = new liblas::Writer(*stream, *header);


		//LASheader header;
		//header.clean();
		//header.number_of_point_records = numAccepted;
		//header.point_data_format = 2;
		//header.point_data_record_length = 26;
		////header.set_bounding_box(acceptedAABB.min.x, acceptedAABB.min.y, acceptedAABB.min.z, acceptedAABB.max.x, acceptedAABB.max.y, acceptedAABB.max.z);
		//header.set_bounding_box(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
		//header.x_scale_factor = 0.01;
		//header.y_scale_factor = 0.01;
		//header.z_scale_factor = 0.01;
		//header.x_offset = 0.0f;
		//header.y_offset = 0.0f;
		//header.z_offset = 0.0f;

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

	}

};



//#include <string>
//#include <iostream>
//
//#include "laswriter.hpp"
//#include "lasdefinitions.hpp"
//
//#include "AABB.h"
//#include "PointWriter.hpp"
//
//using std::string;
//
//class LASPointWriter : public PointWriter{
//
//public:
//	string file;
//	LASheader *header;
//	LASwriter *writer;
//	LASquantizer quantizer;
//	LASpoint lp;
//	int numPoints;
//
//	LASPointWriter(string file, LASheader header) {
//		this->file = file;
//		this->header = new LASheader(header);
//		numPoints = 0;
//
//		LASwriteOpener lwrOpener;
//		lwrOpener.set_file_name(file.c_str());
//		writer = lwrOpener.open(&header);
//
//		quantizer.x_offset = header.x_offset;
//		quantizer.y_offset = header.y_offset;
//		quantizer.z_offset = header.z_offset;
//		quantizer.x_scale_factor = header.x_scale_factor;
//		quantizer.y_scale_factor = header.y_scale_factor;
//		quantizer.z_scale_factor = header.z_scale_factor;
//		
//		lp.init(&quantizer, 2, 26);
//		lp.total_point_size = 26;
//	}
//
//	~LASPointWriter(){
//		close();
//	}
//
//	void write(const Point &point){
//		lp.set_x(point.x);
//		lp.set_y(point.y);
//		lp.set_z(point.z);
//		lp.intensity = point.intensity;
//		lp.classification = point.classification;
//
//		unsigned short rgb[4];
//		rgb[0] = point.r * 256;
//		rgb[1] = point.g * 256;
//		rgb[2] = point.b * 256;
//		lp.set_rgb(rgb);
//
//		writer->write_point(&lp);
//
//		numPoints++;
//	}
//
//	void close(){
//		if(writer != NULL){
//			header->number_of_point_records = numPoints;
//			header->start_of_waveform_data_packet_record = 0;
//
//			writer->update_header(header, false);
//			writer->close(true);
//			delete writer;
//			writer = NULL;
//		}
//	}
//
//};

#endif


