
#pragma once

#include <string>
#include <chrono>
#include <cstdarg>
#include <sstream>

using std::string;
using std::stringstream;

string stringReplace(string str, string search, string replacement);

// see https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
bool icompare_pred(unsigned char a, unsigned char b);

// see https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
bool icompare(std::string const& a, std::string const& b);

bool endsWith(const string& str, const string& suffix);

bool iEndsWith(const std::string& str, const std::string& suffix);

double now();

void printElapsedTime(string label, double startTime);

void printThreadsafe(string str);
void printThreadsafe(string str1, string str2);
void printThreadsafe(string str1, string str2, string str3);
void printThreadsafe(string str1, string str2, string str3, string str4);


