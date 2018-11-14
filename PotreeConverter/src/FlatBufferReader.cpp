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
//                std::cout<<"p values ==  "<<p<<std::endl;
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
                if (!flat){
                    pointcloud = LIDARWORLD::GetPointCloud(buf2);
                    pos = pointcloud->points();
                    pos_len = pos->Length();
                    if(pos_len == 0)
                        total_points_count += pos_len;
                    return true;}
                else if (flat){
                    track = flatbuffers::GetRoot<Flatbuffer::GroundTruth::Track>(buf2);

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


    bool FlatBufferReader::bboxPoint() {
        point.position.x = bbox->Get(j)->x();
        point.position.y = bbox->Get(j)->y();
        point.position.z = bbox->Get(j)->z();
        point.gpsTime = ts;
        return true;
    }
    bool FlatBufferReader::bboxReader() {
        if (j < vec_len) {
            bboxPoint();
            j++;
            std::cout << point.position.x << " " << point.position.y << " " << point.position.z
                      << std::endl;

        } else { j = 0;
            i++;
            bboxState();
            bboxPoint();
        }
    }
    bool FlatBufferReader::bboxState() {
        auto &state = *statesFb;
        if (i < states_len) {
            ts = state[i]->timestamps();
            bbox = state[i]->bbox();
            vec_len = bbox->Length();
            bboxReader();


        } else { i = 0;j=0;
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


                return true;
            } else if (count == pos_len) {
                count = 1;
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
            }
            else if (flat ) {

                bboxState();

                return true;
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


