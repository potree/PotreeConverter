
#include "logger.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <sstream>
#include <mutex>
#include <thread>

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

void info(string msg, string file, int line) {

	string filename = fs::path(file).filename().string();

	stringstream ss;
	ss << "INFO(" << filename << ":" << line << "): " << msg;

	string str = ss.str();

	//cout << str << endl;

	if (fout != nullptr) {
		lock_guard<mutex> lock(mtx);
		*fout << str << endl;
	}


}

void warn(string msg, string file, int line) {

	string filename = fs::path(file).filename().string();

	stringstream ss;
	ss << "WARN(" << filename << ":" << line << "): " << msg;

	string str = ss.str();

	cout << str << endl;

	if (fout != nullptr) {
		lock_guard<mutex> lock(mtx);
		*fout << str << endl;
	}

}


void error(string msg, string file, int line) {

	string filename = fs::path(file).filename().string();

	stringstream ss;
	ss << "ERROR(" << filename << ":" << line << "): " << msg;

	string str = ss.str();

	cout << str << endl;

	if (fout != nullptr) {
		lock_guard<mutex> lock(mtx);
		*fout << str << endl;
	}

}

}