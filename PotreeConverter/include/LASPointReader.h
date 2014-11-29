

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
private:
    double tr[16];
    bool hasTransform;
    Point transform(double x, double y, double z) const {
        Point p;
        if (hasTransform) {
            p.x = tr[0] * x + tr[4] * y + tr[8] * z + tr[12];
            p.y = tr[1] * x + tr[5] * y + tr[9] * z + tr[13];
            p.z = tr[2] * x + tr[6] * y + tr[10] * z + tr[14];
        } else {
            p.x = x;
            p.y = y;
            p.z = z;
        }
        return p;
    }
public:

	ifstream stream;
	liblas::Reader reader;

    LIBLASReader(string path)
            : stream(path, std::ios::in | std::ios::binary),
              reader(liblas::ReaderFactory().CreateWithStream(stream)) {
        std::vector<liblas::VariableRecord> vlrs = reader.GetHeader().GetVLRs();

		hasTransform = false;

//      cout << "There are " << vlrs.size() << " VLRs." << endl;
        for (int i = 0; i < vlrs.size(); ++i) {
            liblas::VariableRecord vlr = vlrs[i];
            if (vlr.GetRecordId() == 2001) {
                const uint8_t *data = vlr.GetData().data();
                for (int k = 0; k < 16; ++k) {
                    memcpy(tr + k, data + (144 + k * 8), sizeof(double));
                }
                hasTransform = true;
                break;
//                cout << "Found a PTX transformation matrix.\n";
            }
        }
    }

	~LIBLASReader(){
		if(stream.is_open()){
			stream.close();
		}
	}

    Point GetPoint() {
        liblas::Point const lp = reader.GetPoint();
        Point p = transform(lp.GetX(), lp.GetY(), lp.GetZ());
        p.intensity = lp.GetIntensity();
        p.classification = lp.GetClassification().GetClass();

        p.r = lp.GetColor().GetRed() / 256;
        p.g = lp.GetColor().GetGreen() / 256;
        p.b = lp.GetColor().GetBlue() / 256;
        return p;
    }
	void close(){
		stream.close();
	}

	AABB getAABB();
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