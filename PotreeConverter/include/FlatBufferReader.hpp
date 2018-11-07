//
// Created by Karthik Sivarama Krishnan on 10/24/18.
//

#ifndef VERITAS_FLATBUFFERREADER_H
#define VERITAS_FLATBUFFERREADER_H

#include "AABB.h"
#include "Point.h"
#include "PointReader.h"
#include "PointAttributes.hpp"
#include "BoostBINPointReader.hpp"

#include <memory>
#include <fstream>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <DataSchemas/LidarWorld_generated.h>


using std::string;

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;


namespace Potree{

    class FlatBufferReader : public PointReader{
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

        FlatBufferReader(string path, AABB aabb, double scale, PointAttributes pointAttributes);

        ~FlatBufferReader();

        bool readNextPoint();

        bool populatePointCloud();

        Point getPoint();

        AABB getAABB();
        int32_t numbytes;

        long long numPoints();
        //LIDAR::PointCloud pointcloud;
        int count;
        int pos_len;
        double filesize;
        double total_points_count;
        Point p;
        void close();

        unsigned char *buffer;
       // unsigned char *buf2;
        Vector3<double> getScale();
    };
}
#endif //VERITAS_FLATBUFFERREADER_H