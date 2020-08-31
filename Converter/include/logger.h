
#pragma once

#include <string>

using std::string;

namespace logger {

	void addOutputFile(string path);

	void info(string msg, string file, int line);

	void warn(string msg, string file, int line);

	void error(string msg, string file, int line);

#define INFO(message) info(message, __FILE__, __LINE__)
#define ERROR(message) error(message, __FILE__, __LINE__)
#define WARN(message) warn(message, __FILE__, __LINE__)

}