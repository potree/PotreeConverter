//
// Created by Karthik Sivarama Krishnan on 10/24/18.
//


#include <fstream>
#include <iostream>
#include <vector>

#include <experimental/filesystem>
#include <DataSchemas/LidarWorld_generated.h>
#include <DataSchemas/GroundTruth_generated.h>



#include "FlatBufferReader.hpp"
#include "stuff.h"

#include "Point.h"
#include "PointReader.h"
#include "PointAttributes.hpp"
#include "BoostBINPointReader.hpp"
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Eigen>


const Flatbuffer::GroundTruth::Track *track;
using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using std::ios;



namespace Potree{


    FlatBufferReader::FlatBufferReader(string path, AABB aabb, double scale, PointAttributes pointAttributes, bool flat_buffer ) : endOfFile(false), count(1), pos_len(0), numbytes(0), filesize(0), total_points_count(0){
        this->path = path;
        this->aabb = aabb;
        this->scale = scale;
        this->attributes = pointAttributes;
        buffer = new unsigned char[4];
        flat= flat_buffer;
        std::cout<<"file type    " <<flat <<std::endl;
        std::cout << "Filepath = " << path << std::endl;

        if(fs::is_directory(path)){
            // if directory is specified, find all las and laz files inside directory

            for(fs::directory_iterator it(path); it != fs::directory_iterator(); it++){
                fs::path filepath = it->path();
                if(fs::is_regular_file(filepath)){
                    files.push_back(filepath.string());
                }
            }
        }else{
            files.push_back(path);
        }

        currentFile = files.begin();
        reader = new ifstream(*currentFile, ios::in | ios::binary);
        bool firstCloudPopulated = populatePointCloud();
        if (!firstCloudPopulated) {
            std::cerr << "Could not populate first cloud" << std::endl;
        }

        // Calculate AABB:
        if (true ) {
            pointCount = 0;
            std::cout<<"AABB function is running now "<<std::endl;
            while(readNextPoint()) {

                p = getPoint();

                if (pointCount == 0) {
                    this->aabb = AABB(p.position);
                } else {
                    this->aabb.update(p.position);
                }
                pointCount++;
            }}

        reader->clear();
        reader->seekg(0, reader->beg);
        currentFile = files.begin();
        reader = new ifstream(*currentFile, ios::in | ios::binary);
        std::cout <<"The Total Available points in the File  =  "<< pointCount << std::endl;

    }

    FlatBufferReader::~FlatBufferReader(){

        close();

    }

    void FlatBufferReader::close(){
//    delete[] buffer;
        if(reader != NULL){
            reader->close();
            delete[] buffer;
            delete reader;
            reader = NULL;

        }
    }

    long long FlatBufferReader::numPoints(){
        return pointCount;
    }

    bool FlatBufferReader::populatePointCloud()  {
        try{
            std::cout.precision(std::numeric_limits<double>::max_digits10);
//            buffer = new unsigned char[4];

            reader->read(reinterpret_cast<char *>(buffer), 4);


            numbytes = (uint32_t) buffer[3] << 24 |
                       (uint32_t) buffer[2] << 16 |
                       (uint32_t) buffer[1] << 8 |
                       (uint32_t) buffer[0];
            filesize+= numbytes;
            if (numbytes==0){
                endOfFile = false;

                std::cout << "END OF FILE REACHED" << std::endl;
                return false;
            }
            else{
//                char *buf2 = new char[numbytes];
                buf2.clear();
                buf2.reserve(numbytes);
                if (reader->eof()){
                    std::cerr << "Reader is at end of file" << std::endl;
                    endOfFile = false;
                    return false;
                }
                reader->read(&buf2[0], numbytes);
                if (!flat){
                    pointcloud = LIDARWORLD::GetPointCloud(&buf2[0]);
                    pos = pointcloud->points();
                    pos_len = pos->Length();
                    if(pos_len == 0)
                        total_points_count += pos_len;
                    return true;
                }
                else if (flat){
                    track = flatbuffers::GetRoot<Flatbuffer::GroundTruth::Track>(&buf2[0]);

                    statesFb = track->states();
                    states_len = statesFb->Length();
                    return  true;}


            }
        }
        catch (std::exception& e) {
            endOfFile = false;
            std::cout << "No More Points Left" << std::endl;
            return false;
        }

    }

    Eigen::Quaterniond
    euler2Quaternion( const double roll,
                      const double pitch,
                      const double yaw ) {
        // TODO check yaw pitch roll order required in use case
        Eigen::AngleAxisd rollAngle(roll, Eigen::Vector3d::UnitX());
        Eigen::AngleAxisd pitchAngle(pitch, Eigen::Vector3d::UnitY());
        Eigen::AngleAxisd yawAngle(yaw, Eigen::Vector3d::UnitZ());

        Eigen::Quaterniond q = yawAngle *pitchAngle *rollAngle;
        return q;
    }

    Eigen::Matrix4d getTxMat(Eigen::Vector4d dx, Eigen::Vector3d dTheta){
        Eigen::Matrix3d rot_mat = euler2Quaternion(dTheta.x(),dTheta.y(),dTheta.z()).toRotationMatrix();
        Eigen::Matrix4d Trans; // Transformation matrix
        Trans.setIdentity();
        Trans.block<3,3>(0,0) = rot_mat;
        Trans.rightCols<1>() = dx;

        return Trans;
    }

    Vector3<double> FlatBufferReader::centroid() {

        for(int jj=0;jj<vec_len;jj++){
            centroid_x= centroid_x+bbox->Get(jj)->x();
            centroid_y= centroid_y+bbox->Get(jj)->y();
            centroid_z= centroid_z+bbox->Get(jj)->z();
        }
        Centroidbbox.x= centroid_x/8;Centroidbbox.y= centroid_y/8;Centroidbbox.z= centroid_z/8;
        centroid_x=centroid_y=centroid_z=0;

    }
    bool FlatBufferReader::bboxPoint() {

        New(0)= Centroidbbox.x-bbox->Get(bboxidx)->x();
        New(1)= Centroidbbox.y-bbox->Get(bboxidx)->y();
        New(2)= Centroidbbox.z-bbox->Get(bboxidx)->z();
        New(3)=1;
        Yaw(0)=0; Yaw(1)=0; Yaw(2)=ya;
        auto RotationMatrix=  getTxMat(New,Yaw);
        RotatedPoint= (RotationMatrix * New);
        newX=RotatedPoint(0)+Centroidbbox.x;
        newY=RotatedPoint(1)+Centroidbbox.y;
        newZ=RotatedPoint(2)+Centroidbbox.z;

        point.position.x = newX;
        point.position.y = newY;
        point.position.z = newZ;
        point.gpsTime = ts;
        newX=0 , newY=0,newZ=0;
        return true;
    }
    bool FlatBufferReader::bboxReader() {


        if (bboxidx < vec_len) {
            if(bboxidx==0){centroid();}
            bboxPoint();
            bboxidx++;


        } else { bboxidx = 0;
            statesidx++;
            bboxState();
            bboxPoint();
        }
    }
    bool FlatBufferReader::bboxState() {
        auto &state = *statesFb;
        if (statesidx < states_len) {
            ts = state[statesidx]->timestamps();
            ya = state[statesidx]->yaw();
            bbox = state[statesidx]->bbox();
            vec_len = bbox->Length();

            bboxReader();


        } else { statesidx = 0;bboxidx=0;
            populatePointCloud();
            bboxState();}
    }

    bool FlatBufferReader::readNextPoint() {
        bool hasPoints = reader->good();


        if (!hasPoints) {
            currentFile++;

            if (currentFile != files.end()) {
                // try to open next file, if available
                reader->close();

                delete reader;
                reader = NULL;

                reader = new ifstream(*currentFile, ios::in | ios::binary);
                hasPoints = reader->good();
            }
        }

        if(hasPoints && !endOfFile) {


            if (count < pos_len) {

                auto fbPoints = pos->Get(count);
                count++;

                point.position.x = fbPoints->x();
                point.position.y = fbPoints->y();
                point.position.z = fbPoints->z();
                point.gpsTime = fbPoints->timestamp();
                point.intensity = fbPoints->intensity();
                //delete[] buffer;

                return true;
            } else if (count == pos_len) {
                count = 0;
                if (populatePointCloud()) {
                    auto fbPoints = pos->Get(count);

                    count++;

                    point.position.x = fbPoints->x();
                    point.position.y = fbPoints->y();
                    point.position.z = fbPoints->z();
                    point.gpsTime = fbPoints->timestamp();
                    point.intensity = fbPoints->intensity();

                    return true;

                }
                else{endOfFile = false;
                    std::cout << "I think its the end of file" << std::endl;
                    return false;}
            }
            else if (flat ) {

                bboxState();

            }
            else{endOfFile = false;
                std::cout << "I think its the end of file" << std::endl;
                return false;}


        }
        return hasPoints;
    }



    Point FlatBufferReader::getPoint() {
        return point;
    }

    AABB FlatBufferReader::getAABB() {
        return aabb;
    }


}


