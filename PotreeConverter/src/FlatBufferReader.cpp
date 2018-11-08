//
// Created by Karthik Sivarama Krishnan on 10/24/18.
//


#include <fstream>
#include <iostream>
#include <vector>

#include <experimental/filesystem>
#include <DataSchemas/LidarWorld_generated.h>


#include "FlatBufferReader.hpp"
#include "stuff.h"

#include "Point.h"
#include "PointReader.h"
#include "PointAttributes.hpp"
#include "BoostBINPointReader.hpp"

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using std::ios;


const flatbuffers::Vector<const LIDARWORLD::Point *> *pos;
const LIDARWORLD::PointCloud *pointcloud;
char *buf2;
ifstream **pointer;
namespace Potree{


    FlatBufferReader::FlatBufferReader(string path, AABB aabb, double scale, PointAttributes pointAttributes) : endOfFile(false), count(1), pos_len(0), numbytes(0), filesize(0), total_points_count(0){
        this->path = path;
        this->aabb = aabb;
        this->scale = scale;
        this->attributes = pointAttributes;


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
        if (true) {
            pointCount = 0;
            while(readNextPoint()) {

                p = getPoint();
//                std::cout << pointCount << std::endl;
//            std::cout<<"x=   "<<point.position.x<<"y=   "<<point.position.y<<"z =   "<<point.position.z<<std::endl;
                if (pointCount == 0) {
                    this->aabb = AABB(p.position);
                } else {
                    this->aabb.update(p.position);
                }
                pointCount++;


            }

            reader->clear();
            reader->seekg(0, reader->beg);
//            currentFile = files.begin();
            reader = new ifstream(*currentFile, ios::in | ios::binary);
            std::cout <<"The Total Available points in the File  =  "<< pointCount << std::endl;
        }

    }

    FlatBufferReader::~FlatBufferReader(){
//        delete [] buffer;
//        delete [] buf2;
        close();

    }

    void FlatBufferReader::close(){

        //  delete reader, buffer, buf2;
        if(reader != NULL){
            reader->close();
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
            buffer = new unsigned char[4];

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
                char *buf2 = new char[numbytes];
                if (reader->eof()) {
                    std::cerr << "Reader is at end of file" << std::endl;
                    endOfFile = false;
                    return false;
                }
                reader->read(buf2, numbytes);
                pointcloud = LIDARWORLD::GetPointCloud(buf2);
                pos = pointcloud->points();
                pos_len = pos->Length();
                if(pos_len == 0)
                    total_points_count += pos_len;
                //   std::cout<<"total poslength"<<total_points_count<<std::endl;
                return true;}
        }
        catch (std::exception& e) {
            endOfFile = false;
            std::cout << "No More Points Left" << std::endl;
            return false;
        }

    }

    bool FlatBufferReader::readNextPoint() {
        bool hasPoints = reader->good();


        if(!hasPoints ){
            currentFile++;

            if(currentFile != files.end()){
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


                return true;
            } else if (count == pos_len) {
                count = 1;
                if( populatePointCloud()){
                    auto fbPoints = pos->Get(count);
                    count++;

                    point.position.x = fbPoints->x();
                    point.position.y = fbPoints->y();
                    point.position.z = fbPoints->z();
                    point.gpsTime = fbPoints->timestamp();
                    point.intensity = fbPoints->intensity();


                    return true;
                }


                else {
                    endOfFile = false;

                    std::cout << "END OF FILE REACHED (in READ NEXT POINT)" << std::endl;


                    return false;
                }
            }

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



