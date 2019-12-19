
#include "stuff.h"

#include <iostream>

using std::cout;
using std::endl;

string stringReplace(string str, string search, string replacement) {

	auto index = str.find(search);

	if (index == str.npos) {
		return str;
	}

	string strCopy = str;
	strCopy.replace(index, search.length(), replacement);

	return strCopy;
}

// see https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
bool icompare_pred(unsigned char a, unsigned char b) {
	return std::tolower(a) == std::tolower(b);
}

// see https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
bool icompare(std::string const& a, std::string const& b) {
	if (a.length() == b.length()) {
		return std::equal(b.begin(), b.end(), a.begin(), icompare_pred);
	} else {
		return false;
	}
}

bool endsWith(const string& str, const string& suffix) {

	if (str.size() < suffix.size()) {
		return false;
	}

	auto tstr = str.substr(str.size() - suffix.size());

	return tstr.compare(suffix) == 0;
}

bool iEndsWith(const std::string& str, const std::string& suffix) {

	if (str.size() < suffix.size()) {
		return false;
	}

	auto tstr = str.substr(str.size() - suffix.size());

	return icompare(tstr, suffix);
}

static long long unsuc_start_time = std::chrono::high_resolution_clock::now().time_since_epoch().count();

double now() {
	auto now = std::chrono::high_resolution_clock::now();
	long long nanosSinceStart = now.time_since_epoch().count() - unsuc_start_time;

	double secondsSinceStart = double(nanosSinceStart) / 1'000'000'000;

	return secondsSinceStart;
}


void printThreadsafe(string str) {

	stringstream ss;
	ss << str << endl;

	cout << ss.str();

}

void printThreadsafe(string str1, string str2) {
	stringstream ss;
	ss << str1 << str2 << endl;

	cout << ss.str();
}

void printThreadsafe(string str1, string str2, string str3) {
	stringstream ss;
	ss << str1 << str2 << str3 << endl;

	cout << ss.str();
}

void printThreadsafe(string str1, string str2, string str3, string str4) {
	stringstream ss;
	ss << str1 << str2 << str3 << str4 << endl;

	cout << ss.str();
}
