#include <fstream>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>

#include "PTXPointReader.h"

using std::cout;
using std::endl;
using std::vector;
using std::ios;
using std::string;
using namespace boost::algorithm;

static const int INVALID_INTENSITY = 32767;

std::map<string, AABB> PTXPointReader::aabbs = std::map<string, AABB>();
std::map<string, long> PTXPointReader::counts = std::map<string, long>();

inline void split(vector<double> &v, char (&str)[512]) {
    vector<std::pair<string::const_iterator, string::const_iterator> > sp;
    if (strlen(str) > 200) return;

    string strstr(str);
    split(sp, strstr, is_space(), token_compress_on);
    for (auto beg = sp.begin(); beg != sp.end(); ++beg) {
        string token(beg->first, beg->second);
        if (!token.empty()) {
            v.push_back(atof(token.c_str()));
        }
    }
}

inline void getlined(fstream *stream, vector<double> &result) {
    char str[512];
    result.clear();
    stream->getline(str, 512);
    split(result, str);
}

inline void skipline(fstream *stream) {
    string str;
    getline(*stream, str);
}

bool assertd(fstream *stream, int i) {
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
    this->stream = new fstream(*(this->currentFile), ios::in);
    this->currentChunk = 0;
    skipline(this->stream);
    loadChunk(this->stream, this->currentChunk, this->tr);
}

void PTXPointReader::scanForAABB() {
    // read bounding box
    double x, y, z, minx, miny, minz, maxx, maxy, maxz, intensity;
    bool firstPoint = true;
    bool pleaseStop = false;
    long currentChunk = 0;
    long count = 0;
    double tr[16];
    vector<double> split;
    for (int i = 0; i < files.size(); i++) {
        fstream *stream=new fstream(files[i], ios::in);
        currentChunk = 0;
        getlined(stream, split);
        while (!pleaseStop) {
            if (1 == split.size()) {
                if (!loadChunk(stream, currentChunk, tr)) {
                    break;
                }
            }
            while (true) {
                getlined(stream, split);
                if (4 == split.size() || 7 == split.size()) {
                    x = split[0];
                    y = split[1];
                    z = split[2];
                    intensity = split[3];
                    if (0.5 != intensity) {
                        Point p = transform(tr, x, y, z);
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
                        count++;
                        if (0 == count % 1000000)
                            cout << "AABB-SCANNING: " << count << " points; " << currentChunk << " chunks" << endl;
                    }
                } else {
                    break;
                }
            }
            if (stream->eof()) {
                pleaseStop = true;
                break;
            }
            currentChunk++;
        }
        stream->close();
    }

    counts[path] = count;
    AABB lAABB(Vector3<double>(minx, miny, minz), Vector3<double>(maxx, maxy, maxz));
    PTXPointReader::aabbs[path] = lAABB;
}

bool PTXPointReader::loadChunk(fstream *stream, long currentChunk, double tr[16]) {
    vector<double> split;

    // The first 5 lines should have respectively 1, 3, 3, 3, 3 numbers each.
    if (!assertd(stream, 1) || !assertd(stream, 3) || !assertd(stream, 3) || !assertd(stream, 3) || !assertd(stream, 3))
        return false;

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[0] = split[0];
    tr[1] = split[1];
    tr[2] = split[2];
    tr[3] = split[3];

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[4] = split[0];
    tr[5] = split[1];
    tr[6] = split[2];
    tr[7] = split[3];

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[8] = split[0];
    tr[9] = split[1];
    tr[10] = split[2];
    tr[11] = split[3];

    getlined(stream, split);
    if (4 != split.size()) {
        return false;
    };
    tr[12] = split[0];
    tr[13] = split[1];
    tr[14] = split[2];
    tr[15] = split[3];
    origin = Vector3<double>(split[0], split[1], split[2]);

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
    if (this->stream->eof()) {
        this->stream->close();
        this->currentFile++;

        if (this->currentFile != files.end()) {
            this->stream = new fstream(*(this->currentFile), ios::in);
            this->currentChunk = 0;
            skipline(stream);
            loadChunk(stream, currentChunk, tr);
        } else {
            return false;
        }
    }
    vector<double> split;
    getlined(stream, split);
    if (1 == split.size()) {
        this->currentChunk++;
        loadChunk(stream, currentChunk, tr);
        getlined(stream, split);
    }
    auto size1 = split.size();
    if (size1 > 3) {
        this->p = transform(tr, split[0], split[1], split[2]);
        double intensity = split[3];
        this->p.intensity = (unsigned short)(65535.0 * intensity);
        this->p.a = 0;
        if (4 == size1) {
            this->p.r = (unsigned char)(intensity * 255.0);
			this->p.g = (unsigned char)(intensity * 255.0);
			this->p.b = (unsigned char)(intensity * 255.0);
        } else if (7 == size1) {
            this->p.r = (unsigned char)(split[4]);
            this->p.g = (unsigned char)(split[5]);
            this->p.b = (unsigned char)(split[6]);
        }
    } else {
        this->p.intensity = INVALID_INTENSITY;
    }
    return true;
}
