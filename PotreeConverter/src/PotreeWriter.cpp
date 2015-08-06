
#include "PotreeWriter.hpp"

namespace Potree{



string Node::filename() const {
	return  this->writer->targetDir + "/data/" + name() + ".las";
}

void Node::flush(){
	if(!needsFlush){
		return;
	}

	selected.insert(selected.end(), store.begin(), store.end());

	string file = this->writer->targetDir + "/data/" + name() + ".las";

	liblas::Header *header = new liblas::Header();
	header->SetDataFormatId(liblas::ePointFormat2);
	header->SetMin(this->aabb.min.x, this->aabb.min.y, this->aabb.min.z);
	header->SetMax(this->aabb.max.x, this->aabb.max.y, this->aabb.max.z);
	header->SetScale(0.001, 0.001, 0.001);
	header->SetPointRecordsCount(53);

	std::ofstream ofs;
	ofs.open(file, ios::out | ios::binary);
	liblas::Writer writer(ofs, *header);
	liblas::Point p(header);

	for(int i = 0; i < this->selected.size(); i++){
		Point &point = this->selected[i];

		p.SetX(point.position.x);
		p.SetY(point.position.y);
		p.SetZ(point.position.z);

		vector<uint8_t> &data = p.GetData();

		unsigned short pr = point.r * 256;
		unsigned short pg = point.g * 256;
		unsigned short pb = point.b * 256;

		liblas::Color color(int(point.r) * 256, int(point.g) * 256, int(point.b) * 256);
		p.SetColor(color);

		writer.WritePoint(p);
	} 

	ofs.close();
	delete header;

	// update point count
	int numPoints = (int)this->selected.size();
	std::fstream stream(file, ios::out | ios::binary | ios::in );
	stream.seekp(107);
	stream.write(reinterpret_cast<const char*>(&numPoints), 4);
	stream.close();

	//clear();
	store.clear();

	this->file = file;
	needsFlush = false;
}





}