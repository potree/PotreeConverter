
#pragma once

#include <string>

using std::string;

class State;

namespace chunker_countsort {

	void doChunking(string path, string targetDir, State& state);

}