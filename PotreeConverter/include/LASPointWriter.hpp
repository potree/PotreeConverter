
#ifndef LASPOINTWRITER_H
#define LASPOINTWRITER_H

#include <string>

#include "laswriter.hpp"
#include "lasdefinitions.hpp"

using std::string;

class LASPointWriter{

public:
	string file;
	LASheader &header;
	LASwriter *writer;
	LASquantizer quantizer;
	LASpoint lp;

	LASPointWriter(string file, LASheader &header) 
		: header(header)
	{
		this->file = file;

		LASwriteOpener lwrOpener;
		lwrOpener.set_file_name(file.c_str());
		writer = lwrOpener.open(&header);

		quantizer.x_offset = header.x_offset;
		quantizer.y_offset = header.y_offset;
		quantizer.z_offset = header.z_offset;
		quantizer.x_scale_factor = header.x_scale_factor;
		quantizer.y_scale_factor = header.y_scale_factor;
		quantizer.z_scale_factor = header.z_scale_factor;
		
		lp.init(&quantizer, 2, 26);
		lp.total_point_size = 26;
	}

	~LASPointWriter(){
		close();
	}

	void write(const Point &point){
		lp.set_x(point.x);
		lp.set_y(point.y);
		lp.set_z(point.z);
		lp.intensity = point.intensity;
		lp.classification = 0;

		unsigned short rgb[4];
		rgb[0] = point.r * 256;
		rgb[1] = point.g * 256;
		rgb[2] = point.b * 256;
		lp.set_rgb(rgb);

		writer->write_point(&lp);
	}

	void close(){
		if(writer != NULL){
			writer->close(false);
			delete writer;
			writer = NULL;
		}
	}

};

#endif


