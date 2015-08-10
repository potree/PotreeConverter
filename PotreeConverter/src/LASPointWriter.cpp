
#include <vector>

#include "LASPointWriter.hpp"

using std::vector;


void LASPointWriter::write(const Point &point){
	liblas::Point lp(header);
	
	lp.SetX(point.position.x);
	lp.SetY(point.position.y);
	lp.SetZ(point.position.z);

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

	numPoints++;
}
