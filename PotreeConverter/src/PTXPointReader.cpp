#include <fstream>
#include <sstream>

#include "PTXPointReader.h"

using std::cout;
using std::endl;
using std::vector;
using std::ios;
using std::string;

static const int COORD_THRESHOLD = 100000;
static const int INVALID_INTENSITY = 32767;

void split(const string &s, vector<double> &v) {
    if (s.length() > 200) return;

    std::stringstream in(s);
    v = vector<double>((std::istream_iterator<double>(in)), std::istream_iterator<double>());
}

void getlined(fstream &stream, vector<double> &result) {
    string str;
    getline(stream, str);
    split(str, result);
}

void skipline(fstream &stream) {
    string str;
    getline(stream, str);
}

bool assertd(fstream &stream, int i) {
    vector<double> tokens;
    getlined(stream, tokens);
    bool result = i == tokens.size();
    return result;
}

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
    skipline(stream);
    loadChunk();
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
        vector<double> split;
        while (!pleaseStop) {
            cout << "PTXPointReader: scanning " << files[i] << " chunk " << currentChunk << endl;
            getlined(stream, split);
            if (1 == split.size() && !loadChunk()) {
                break;
            }
            while (true) {
                if (4 == split.size()) {
                    x = split[0];
                    y = split[1];
                    z = split[2];
                    intensity = split[3];
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
                } else {
                    break;
                }
                getlined(stream, split);
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
    cout << "Loading a new PTX chunk: " << this->currentChunk << endl;

    if (!assertd(stream, 1))return false;
    if (!assertd(stream, 3))return false;
    if (!assertd(stream, 3))return false;
    if (!assertd(stream, 3))return false;
    if (!assertd(stream, 3))return false;

    vector<double> split;
    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[0] = split[0]; tr[1] = split[1]; tr[2] = split[2]; tr[3] = split[3];

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[4] = split[0]; tr[5] = split[1]; tr[6] = split[2]; tr[7] = split[3];

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[8] = split[0]; tr[9] = split[1]; tr[10] = split[2]; tr[11] = split[3];

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[12] = split[0]; tr[13] = split[1]; tr[14] = split[2]; tr[15] = split[3];

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
    if (this->stream.eof()) {
        this->stream.close();
        this->currentFile++;

        if (this->currentFile != files.end()) {
            this->stream = fstream(*(this->currentFile), ios::in);
            this->currentChunk = 0;
            skipline(stream);
            loadChunk();
        } else {
            return false;
        }
    }
    vector<double> split;
    getlined(stream, split);
    if (1 == split.size()) {
        this->currentChunk++;
        skipline(stream);
        loadChunk();
        getlined(stream, split);
    }
    if (4 == split.size()) {
        double x = split[0], y = split[1], z = split[2], i = split[3];
        this->p = transform(x, y, z);
        this->p.intensity = 65535.0 * i;
        p.r = 255;
        p.g = 0;
        p.b = 0;
        p.a = 0;
    } else {
        this->p.intensity = INVALID_INTENSITY;
    }
    return true;
}
