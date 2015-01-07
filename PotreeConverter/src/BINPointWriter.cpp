#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "BINPointWriter.hpp"

namespace fs = boost::filesystem;

using std::ofstream;
using std::cout;
using std::endl;
using std::vector;
using boost::iequals;
using std::ios;

BINPointWriter::BINPointWriter(string file, AABB aabb, double scale) {
    this->file = file;
    this->aabb = aabb;
    this->scale = scale;
    attributes.add(PointAttribute::POSITION_CARTESIAN);
    attributes.add(PointAttribute::COLOR_PACKED);
    if (fs::exists(file)) {
        numPoints = fs::file_size(file) / attributes.byteSize;
        writer = new ofstream(file, ios::out | ios::binary | ios::ate | ios::app);
    } else {
        numPoints = 0;
        writer = new ofstream(file, ios::out | ios::binary);
    }
}

BINPointWriter::BINPointWriter(string file, PointAttributes attributes) {
    this->file = file;
    this->attributes = attributes;

    if (fs::exists(file)) {
        numPoints = fs::file_size(file) / attributes.byteSize;
        writer = new ofstream(file, ios::out | ios::binary | ios::ate | ios::app);
    } else {
        numPoints = 0;
        writer = new ofstream(file, ios::out | ios::binary);
    }
}








