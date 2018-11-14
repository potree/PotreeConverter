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
#include <DataSchemas/GroundTruth_generated.h>


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

    public:

        FlatBufferReader(string path, AABB aabb, double scale, PointAttributes pointAttributes, bool flat_buffer);


        ~FlatBufferReader();
        bool bboxPoint();

        bool readNextPoint();

        bool populatePointCloud();
        bool bboxReader();
        bool bboxState();

        Point getPoint();

        AABB getAABB();
        int32_t numbytes;

        long long numPoints();
        bool flat;
        int count, i=0,j=0,pointCounts = 0;
        int pos_len, vec_len,states_len ;
        double filesize;
        double total_points_count, total_vec_count, ts;
        Point p;
        void close();

        const flatbuffers::Vector<const LIDARWORLD::Point *> *pos;
        const LIDARWORLD::PointCloud *pointcloud;
        const Flatbuffer::GroundTruth::State *states;
        char *buf2;
        ifstream **pointer;
        const flatbuffers::Vector<const Flatbuffer::GroundTruth::Vec3 *> *vec;
        const flatbuffers::Vector<const Flatbuffer::GroundTruth::Vec3 *> *bbox;

        const flatbuffers::Vector<flatbuffers::Offset<Flatbuffer::GroundTruth::State>> *statesFb;

        unsigned char *buffer;
       // unsigned char *buf2;
        Vector3<double> getScale();
    };
}
#endif //VERITAS_FLATBUFFERREADER_H