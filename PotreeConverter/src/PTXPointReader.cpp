#include <fstream>
#include <iostream>
#include <vector>

#include "boost/filesystem.hpp"
#include <boost/algorithm/string.hpp>
#include <term.h>

#include "PTXPointReader.h"


namespace fs = boost::filesystem;

using std::ifstream;
using std::fstream;
using std::cout;
using std::endl;
using std::vector;
using boost::iequals;
using std::ios;

static const int COORD_THRESHOLD = 10000;
static const int INVALID_INTENSITY = 32767;

/**
* The constructor needs to scan the whole PTX file to find out the bounding box. Unluckily.
* TODO: during the scan all the points are read and transformed. Afterwards, during loading
* the points are read again and transformed again. It should be nice to save the
* transformed points in a temporary file. That would mean a LAS file or something similar.
* Unuseful. It's better to convert the PTX to LAS files.
* TODO: it seems theat the PTXPointReader is asked to produce the bounding box more than once.
* Maybe it should be saved somewhere. Chez moi, scanning 14m points needs 90 secs. The
* process speed of the PTX file is about 1m points every 50 secs.
*/
PTXPointReader::PTXPointReader(string path) {
    this->path = path;

    if (fs::is_directory(path)) {
        // if directory is specified, find all ptx files inside directory

        for (fs::directory_iterator it(path); it != fs::directory_iterator(); it++) {
            fs::path filepath = it->path();
            if (fs::is_regular_file(filepath)) {
                if (iequals(fs::extension(filepath), ".ptx")) {
                    files.push_back(filepath.string());
                }
            }
        }
    } else {
        files.push_back(path);
    }

    // open first file
    this->currentFile = files.begin();
    stream = fstream(*(this->currentFile), ios::in);
    this->currentChunk = 0;
    loadChunk();

//    cout << "let's go..." << endl;
}

void PTXPointReader::scanForAABB() {
    // read bounding box
    double x, y, z, dummy, minx, miny, minz, maxx, maxy, maxz, intensity;
    bool firstPoint = true;
    bool pleaseStop = false;
    this->count = 0;
    cout << "PTXPointReader: scanning points for AABB." << endl;
    for (int i = 0; i < files.size(); i++) {
        this->stream = fstream(files[i], ios::in);
        this->currentChunk = 0;
        while (!pleaseStop) {
            cout << "PTXPointReader: scanning " << files[i] << " chunk " << currentChunk << endl;
            if (!loadChunk()) {
                break;
            }
            for (int j = 0; j < this->pointsInCurrentChunk; j++) {
                this->stream >> x >> y >> z >> intensity;
                if (0.5 != intensity) {
                    Point p = transform(x, y, z);
                    if (firstPoint) {
                        maxx = minx = p.x;
                        maxy = miny = p.y;
                        maxz = minz = p.z;
                        firstPoint = false;
                    } else {
                        minx = p.x < minx ? p.x : minx;
                        maxx = p.x > maxx ? p.x : maxx;
                        miny = p.y < miny ? p.y : miny;
                        maxy = p.y > maxy ? p.y : maxy;
                        minz = p.z < minz ? p.z : minz;
                        maxz = p.z > maxz ? p.z : maxz;
                    }
                    this->count++;
                    if (0 == this->count % 1000000)
                        cout << "PTXPointReader: scanned " << count / 1000000 << "m points.\n";
                }
            }
            if (this->stream.eof()) {
                pleaseStop = true;
                break;
            }
            this->currentChunk++;
        }
        this->stream.close();
    }

    cout << "PTXPointReader: scanning points for AABB done." << endl;

    AABB lAABB(Vector3<double>(minx, miny, minz), Vector3<double>(maxx, maxy, maxz));
    aabb.update(lAABB.min);
    aabb.update(lAABB.max);
}

bool PTXPointReader::loadChunk() {
    long rows, cols;
    double dummy;
    cout << "Loading a new PTX chunk: " << this->currentChunk << endl;

    this->currentPointInChunk = 0;
    this->stream >> cols >> rows;
    if (this->stream.eof())
        return false;
    this->pointsInCurrentChunk = cols * rows;
    for (int j = 0; j < 4; j++) {
        this->stream >> dummy >> dummy >> dummy;
    }
    this->stream >> tr[0] >> tr[1] >> tr[2] >> tr[3]
            >> tr[4] >> tr[5] >> tr[6] >> tr[7]
            >> tr[8] >> tr[9] >> tr[10] >> tr[11]
            >> tr[12] >> tr[13] >> tr[14] >> tr[15];
    return true;
}

bool PTXPointReader::readNextPoint() {
    while (true) {
        bool result = doReadNextPoint();
        if (!result)
            return false;
        if (INVALID_INTENSITY != p.intensity)
            return true;
    }
    return false;
}

bool PTXPointReader::doReadNextPoint() {
    if (this->currentPointInChunk == this->pointsInCurrentChunk) {
        if (this->stream.eof()) {
            this->stream.close();
            this->currentFile++;
            this->currentPointInChunk = 0;

            if (this->currentFile != files.end()) {
                this->stream = fstream(*(this->currentFile), ios::in);
                this->currentChunk = 0;
                loadChunk();
            } else {
                return false;
            }
        } else {
            this->currentChunk++;
            loadChunk();
        }
    }
    double x, y, z, i;
    this->stream >> x >> y >> z >> i;
    this->currentPointInChunk++;
    this->p = transform(x, y, z);
    if (x > COORD_THRESHOLD || y > COORD_THRESHOLD || z > COORD_THRESHOLD)
        this->p.intensity = INVALID_INTENSITY;
    else
        this->p.intensity = 65535.0 * i;
    return true;
}
