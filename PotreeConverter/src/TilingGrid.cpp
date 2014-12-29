#include <iostream>
#include <stuff.h>

#include "TilingGrid.h"

TilingGrid::TilingGrid(float spacing, double scale, string tilesDir) {
    this->spacing = spacing;
    this->scale = scale;
    this->tilesDir = tilesDir;
    numAccepted = 0;
}

TilingGrid::~TilingGrid() {
    flush();
}

void TilingGrid::add(Point &point) {
    int nx = (int) (point.x / spacing);
    int ny = (int) (point.y / spacing);
    int nz = (int) (point.z / spacing);

    GridIndex index(nx, ny, nz);
    if (find(index) == end()) {
        AABB myAABB(Vector3<double>(nx * spacing, ny * spacing, nz * spacing),
                Vector3<double>((nx + 1) * spacing, (ny + 1) * spacing, (nz + 1) * spacing));
        string fileName = tilesDir + "/t_" +
                boost::lexical_cast<std::string>(nx) + "_" +
                boost::lexical_cast<std::string>(ny) + "_" +
                boost::lexical_cast<std::string>(nz) + ".las";
        this->operator[](index) = new LASPointWriter(fileName, myAABB, scale);
    }
    this->operator[](index)->write(point);
}

void TilingGrid::flush() {
    TilingGrid::iterator it;
    for (it = begin(); it != end(); it++) {
        delete it->second;
    }
    this->clear();
}