
#include "stuff.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

using std::stringstream;
using std::cout;
using std::endl;
using std::ofstream;
using std::ifstream;
using std::vector;

string repeat(string str, int count) {

	stringstream ss;

	for (int i = 0; i < count; i++) {
		ss << str;
	}

	return ss.str();
}

void writeFile(string path, string text) {

	ofstream out;
	out.open(path);

	out << text;

	out.close();
}

// taken from: https://stackoverflow.com/questions/18816126/c-read-the-whole-file-in-buffer
vector<char> readBinaryFile(string path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	
	return buffer;
}


// taken from: https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602060
string readTextFile(string path) {

	std::ifstream t(path);
	std::string str;

	t.seekg(0, std::ios::end);
	str.reserve(t.tellg());
	t.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(t)),
		std::istreambuf_iterator<char>());

	return str;
}

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

void printElapsedTime(string label, double startTime) {

	double elapsed = now() - startTime;

	cout << label << ": " << elapsed << "s" << endl;

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


#include <Windows.h>
#include "psapi.h"

MemoryUsage getMemoryUsage() {

	MemoryUsage usage;

	{
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);

		usage.totalMemory = memInfo.ullTotalPhys;

	}

	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)& pmc, sizeof(pmc));
		SIZE_T virtualMemUsedByMe = pmc.PrivateUsage;

		usage.usedMemory = pmc.WorkingSetSize;

	}


	return usage;
}