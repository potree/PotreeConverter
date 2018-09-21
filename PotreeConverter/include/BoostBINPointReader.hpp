

#ifndef BOOSTBINPOINTREADER_H
#define BOOSTBINPOINTREADER_H

#include <string>
#include <iostream>
#include <vector>

#include "AABB.h"
#include "Point.h"
#include "PointReader.h"
#include "PointAttributes.hpp"

#include <memory>
#include <fstream>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>


using std::string;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;

struct LidarScan {
    double easting;
    double northing;
    double altitude;
    double intensity;
    double gpsTime;

    friend std::ostream& operator<< (std::ostream &out, const LidarScan &scan) {
        out.precision(std::numeric_limits<double>::max_digits10);
        out << scan.easting << " || "
            << scan.northing << " || "
            << scan.altitude << " || "
            << scan.intensity << " || "
            << scan.gpsTime;
        return out;
    }
};

namespace boost {
    namespace serialization {
        /**
         * @brief Serialization function for LidarScan
         * @tparam Archive The type of the archive (binary or text)
         * @param ar The archive to operate on
         * @param scan The scan to serialize from or deserialize to
         * @param version The version number
         */
        template <class Archive>
        void serialize(Archive &ar, LidarScan &scan, const unsigned int version) {
            ar & scan.easting;
            ar & scan.northing;
            ar & scan.altitude;
            ar & scan.intensity;
            ar & scan.gpsTime;
        }
    }
};

namespace Potree{

class BoostBINPointReader : public PointReader{
private:
	AABB aabb;
	double scale;
	string path;
	vector<string> files;
	vector<string>::iterator currentFile;
	ifstream *reader;
	PointAttributes attributes;
	Point point;
	long pointCount;

	bool endOfFile;
	std::unique_ptr<boost::archive::binary_iarchive> archivePtr;

public:

	BoostBINPointReader(string path, AABB aabb, double scale, PointAttributes pointAttributes);

	~BoostBINPointReader();

	bool readNextPoint();

	Point getPoint();

	AABB getAABB();

	long long numPoints();

	void close();

	Vector3<double> getScale();
};

}

#endif
