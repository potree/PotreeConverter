
#include "logger.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <sstream>
#include <mutex>
#include <thread>
#include <format>
#include <print>

#include "../modules/unsuck/unsuck.hpp"

using std::ofstream;
using std::fstream;
using std::ostream;
using std::shared_ptr;
using std::make_shared;
using std::stringstream;
using std::cout;
using std::endl;
using std::mutex;
using std::lock_guard;
using std::format;
using std::println;

namespace fs = std::filesystem;

namespace logger{

static shared_ptr<std::ofstream> fout = nullptr;

mutex mtx;

void addOutputFile(string path) {

	if (fout != nullptr) {
		fout->close();
	}

	fout = make_shared<ofstream>();
	fout->open(path);

}

string formatTime(double duration){

	int hours = duration / 3600.0;
	int minutes = fmodf(duration, 3600.0) / 60.0;
	int seconds = fmodf(duration, 60.0);

	return format("{}h {}m {}s", hours, minutes, seconds);
}

void info(string msg, string file, int line) {

	string filename = fs::path(file).filename().string();
	string duration = formatTime(now());
	string str = format("INFO({}; {}:{}); {}", duration, filename, line, msg);

	if (fout != nullptr) {
		lock_guard<mutex> lock(mtx);
		*fout << str << endl;
	}


}

void warn(string msg, string file, int line) {

	string filename = fs::path(file).filename().string();
	string duration = formatTime(now());
	string str = format("WARN({}; {}:{}); {}", duration, filename, line, msg);

	cout << str << endl;

	if (fout != nullptr) {
		lock_guard<mutex> lock(mtx);
		*fout << str << endl;
	}

}


void error(string msg, string file, int line) {

	string filename = fs::path(file).filename().string();
	string duration = formatTime(now());
	string str = format("ERROR({}; {}:{}); {}", duration, filename, line, msg);

	cout << str << endl;

	if (fout != nullptr) {
		lock_guard<mutex> lock(mtx);
		*fout << str << endl;
	}

}

}