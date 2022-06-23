
#pragma once

#include <string>
#include <vector>

#include "Vector3.h"
#include "Attributes.h"

using std::string;
using std::vector;

class Source;
class State;

namespace chunker_sparse {

	void run(vector<Source> sources, string targetDir, Vector3 min, Vector3 max, State& state, Attributes outputAttributes);

}