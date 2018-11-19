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
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Eigen>
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

        bool centroid();


        Point getPoint();

        AABB getAABB();
        int32_t numbytes;

        long long numPoints();
        bool flat;
        int count=0, statesidx=0,bboxidx=0,pointCounts = 0,counter=0;
        int pos_len, vec_len,states_len ;
        double filesize;
        double total_points_count, ya, ts;

        Point p;
        double centroid_x=0,centroid_y=0,centroid_z=0;
        double newX=0, newY=0,newZ=0;
        double center=0;
        void close();

        const flatbuffers::Vector<const LIDARWORLD::Point *> *pos;
        const LIDARWORLD::PointCloud *pointcloud;
        const Flatbuffer::GroundTruth::State *states;
        ifstream **pointer;
        const flatbuffers::Vector<const Flatbuffer::GroundTruth::Vec3 *> *vec;
        const flatbuffers::Vector<const Flatbuffer::GroundTruth::Vec3 *> *bbox;
        const Flatbuffer::GroundTruth::Track *track;
        const flatbuffers::Vector<flatbuffers::Offset<Flatbuffer::GroundTruth::State>> *statesFb;

        unsigned char *buffer;
        std::vector<char> buf2;
        std::vector< double>x,y,z,x1,y1,z1;
        Eigen::Vector4d New;
        Vector3<double>Centroidbbox;

    };
}
#endif //VERITAS_FLATBUFFERREADER_H