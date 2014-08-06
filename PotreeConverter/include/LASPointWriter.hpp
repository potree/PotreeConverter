
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
	LASpoint lp;

	LASPointWriter(string file, LASheader &header) 
		: header(header)
	{
		this->file = file;

		LASwriteOpener lwrOpener;
		lwrOpener.set_file_name(file.c_str());
		writer = lwrOpener.open(&header);

		LASquantizer *lq = new LASquantizer();
		lq->x_offset = header.x_offset;
		lq->y_offset = header.y_offset;
		lq->z_offset = header.z_offset;
		lq->x_scale_factor = header.x_scale_factor;
		lq->y_scale_factor = header.y_scale_factor;
		lq->z_scale_factor = header.z_scale_factor;
		
		lp.init(lq, 2, 26);
	}

	void write(const Point &point){
		lp.set_x(point.x);
		lp.set_y(point.y);
		lp.set_z(point.z);
		lp.intensity = point.intensity;
		lp.classification = 0;

		//unsigned short rgb[4];
		//rgb[0] = point.r * 65025;
		//rgb[1] = point.g * 65025;
		//rgb[2] = point.b * 65025;
		//lp.set_rgb(rgb);

		writer->write_point(&lp);
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


