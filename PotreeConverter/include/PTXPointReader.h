#ifndef PTXPOINTREADER_H
#define PTXPOINTREADER_H

#include "PointReader.h"

using std::string;
using std::fstream;
using std::vector;

/**
* This reader importa PTX files. We suppose that PTX files are a concatenation,
* of multiple PTX "chunks", all of them having the same structure. Every point
* has exactly 4 double precision fields: X, Y, Z, Intensity (from 0.0 to 1.0).
*/
class PTXPointReader : public PointReader {
private:
    double tr[16];
    long count;
    Point p;
    long currentChunk;
    bool hasAABB = false;

    inline Point transform(double x, double y, double z) const {
        Point p(tr[0] * x + tr[4] * y + tr[8] * z + tr[12],
                tr[1] * x + tr[5] * y + tr[9] * z + tr[13],
                tr[2] * x + tr[6] * y + tr[10] * z + tr[14]);
        return p;
    }

    fstream stream;
    AABB aabb;
    string path;
    vector<string> files;
    vector<string>::iterator currentFile;

    /**
    * Returns false if there is neo next chunk.
    */
    bool loadChunk();

    void scanForAABB();

    bool doReadNextPoint();

public:

    PTXPointReader(string path);

    ~PTXPointReader() {
        close();
    }

    bool readNextPoint();

    inline Point getPoint() {
        return p;
    }

    inline AABB getAABB() {
        if(!hasAABB) {
            scanForAABB();
            hasAABB = true;
        }
        return aabb;
    }

    inline long numPoints() {
        return count;
    }

    inline void close() {
        stream.close();
    }
};

#endif