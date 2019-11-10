
#pragma once

#include <unordered_map>

using std::unordered_map;

// see LAS spec 1.4
// https://www.asprs.org/wp-content/uploads/2010/12/LAS_1_4_r13.pdf
// total of 192 bytes 
struct ExtraBytesRecord {
	unsigned char reserved[2];  
	unsigned char data_type; 
	unsigned char options;
	char name[32]; 
	unsigned char unused[4]; 
	int64_t no_data[3]; // 24 = 3*8 bytes // hack: not really int, can be double too
	int64_t min[3]; // 24 = 3*8 bytes	  // hack: not really int, can be double too
	int64_t max[3]; // 24 = 3*8 bytes	  // hack: not really int, can be double too
	double scale[3]; 
	double offset[3]; 
	char description[32]; 
}; 

struct ExtraType {
	string type = "";
	int size = 0;
	int numElements = 0;
};

//ExtraType extraTypeFromID(int id) {
//	if (id == 0) {
//		return ExtraType{ "undefined", 0, 1 };
//	}else if (id == 1) { 
//		return ExtraType{ "uint8", 1, 1 };
//	}else if (id == 2) {
//		return ExtraType{ "int8", 1, 1 };
//	}else if (id == 3) {
//		return ExtraType{ "uint16", 2, 1 };
//	}else if (id == 4) {
//		return ExtraType{ "int16", 2, 1 };
//	}else if (id == 5) {
//		return ExtraType{ "uint32", 4, 1 };
//	}else if (id == 6) {
//		return ExtraType{ "int32", 4, 1 };
//	}else if (id == 7) {
//		return ExtraType{ "uint64", 8, 1 };
//	}else if (id == 8) {
//		return ExtraType{ "int64", 8, 1 };
//	}else if (id == 9) {
//		return ExtraType{ "float", 4, 1 };
//	}else if (id == 10) {
//		return ExtraType{ "double", 8, 1 };
//	}
//
//	cout << "ERROR: unsupported extra type: " << id << endl;
//	exit(123);
//}

const unordered_map<unsigned char, ExtraType> typeToExtraType = {
	{0, ExtraType{"undefined", 0, 1}},
	{1, ExtraType{"uint8", 1, 1}},
	{2, ExtraType{"int8", 1, 1}},
	{3, ExtraType{"uint16", 2, 1}},
	{4, ExtraType{"int16", 2, 1}},
	{5, ExtraType{"uint32", 4, 1}},
	{6, ExtraType{"int32", 4, 1}},
	{7, ExtraType{"uint64", 8, 1}},
	{8, ExtraType{"int64", 8, 1}},
	{9, ExtraType{"float", 4, 1}},
	{10, ExtraType{"double", 8, 1}},
};




