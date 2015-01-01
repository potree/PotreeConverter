#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "PotreeTiler.h"
#include "stuff.h"
#include "PotreeException.h"
#include "PotreeWriter.h"
#include "LASPointWriter.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <math.h>
#include <fstream>
#include "TilingGrid.h"

using std::stringstream;
using std::map;
using std::string;
using std::vector;
using std::find;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::fstream;
using boost::iends_with;
using boost::filesystem::is_directory;
using boost::filesystem::directory_iterator;
using boost::filesystem::is_regular_file;
using boost::filesystem::path;

PotreeTiler::PotreeTiler(vector<string> sources, string workDir, float spacing, double scale) {
    // if sources contains directories, use files inside the directory instead
    vector<string> sourceFiles;
    for (int i = 0; i < sources.size(); i++) {
        string source = sources[i];
        path pSource(source);
        if (boost::filesystem::is_directory(pSource)) {
            boost::filesystem::directory_iterator it(pSource);
            for (; it != boost::filesystem::directory_iterator(); it++) {
                path pDirectoryEntry = it->path();
                if (boost::filesystem::is_regular_file(pDirectoryEntry)) {
                    string filepath = pDirectoryEntry.string();
                    if (boost::iends_with(filepath, ".las") || boost::iends_with(filepath, ".laz") || boost::iends_with(filepath, ".ply") || boost::iends_with(filepath, ".ptx")) {
                        sourceFiles.push_back(filepath);
                    }
                }
            }
        } else if (boost::filesystem::is_regular_file(pSource)) {
            sourceFiles.push_back(source);
        }
    }

    this->sources = sourceFiles;
    this->workDir = workDir;
    this->spacing = spacing;
    this->scale = scale;

    boost::filesystem::path tilesDir(workDir + "/tiles");
    boost::filesystem::create_directories(tilesDir);
}

void PotreeTiler::tile() {
    auto start = high_resolution_clock::now();

    TilingGrid grid(spacing, scale, workDir + "/tiles");

    long long pointsProcessed = 0;
    for (int i = 0; i < sources.size(); i++) {
        string source = sources[i];
        cout << "tiling " << source << endl;

        PointReader *reader = createPointReader(source);
        while (reader->readNextPoint()) {
            pointsProcessed++;
            //if((pointsProcessed%50) != 0){
            //	continue;
            //}

            Point p = reader->getPoint();
            grid.add(p);

            if ((pointsProcessed % 1000000) == 0) {
                cout << (pointsProcessed / 1000000) << "m points in ";
                auto end = high_resolution_clock::now();
                long duration = duration_cast<milliseconds>(end - start).count();
                cout << (duration / 1000.0f) << "s" << endl;
                if ((pointsProcessed % 10000000) == 0)
                    grid.flush();
                //return;
            }
        }
        reader->close();
        delete reader;
    }

    grid.flush();

//	cout << writer.numAccepted << " points written" << endl;

    auto end = high_resolution_clock::now();
    long duration = duration_cast<milliseconds>(end - start).count();
    cout << "duration: " << (duration / 1000.0f) << "s" << endl;

    cout << "closing writer" << endl;
}
