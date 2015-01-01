#ifndef POTREE_TILER_H
#define POTREE_TILER_H

#include "AABB.h"
#include "definitions.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cstdint>

class SparseGrid;


using std::vector;
using std::string;
using std::stringstream;
using std::map;

class PotreeTiler {

private:
    vector<string> sources;
    string workDir;
    float spacing;
    double scale;

public:

    PotreeTiler(vector<string> fData, string workDir, float spacing, double scale);

    void tile();
};


#endif
