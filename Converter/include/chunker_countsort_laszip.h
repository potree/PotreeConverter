
#pragma once

#include <string>
#include <vector>

#include "Vector3.h"
#include "Attributes.h"
#include "Monitor.h"

using std::string;
using std::vector;

class Source;
class State;

namespace chunker_countsort_laszip {

	void doChunking(vector<Source> sources, string targetDir, Vector3 min, Vector3 max, State& state, Attributes outputAttributes, Monitor* monitor);
	void fixBoundingBox(vector<Source> sources, Vector3& min, Vector3& max, Attributes& outputAttributes);
}
