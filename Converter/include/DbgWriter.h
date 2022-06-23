
#pragma once

#include <filesystem>

#include "structures.h"

namespace fs = std::filesystem;

namespace dbgwriter{

	struct Point {
		int64_t x;
		int64_t y;
		int64_t z;
	};


	vector<Point> getPoints(Node* node, Attributes attributes);

	std::pair<Point, Point> computeBox(vector<Point> points);

	void writePosition2(string dir, Node* node, Attributes attributes);

	void writePosition(string dir, Node* node, Attributes attributes);

	// method to seperate bits from a given integer 3 positions apart
	inline uint64_t splitBy3(unsigned int a);

	inline uint64_t mortonEncode_magicbits(unsigned int x, unsigned int y, unsigned int z);

	void writePositionMortonCode(string dir, Node* node, Attributes attributes);

	void writeIntensity(string dir, Node* node, Attributes attributes);


	void writeNormal(string dir, Node* node, Attributes attributes, Attribute attribute);

	void writeRGB(string dir, Node* node, Attributes attributes, Attribute attribute);

	void writeDifference(string dir, Node* node, Attributes attributes, Attribute attribute);

	void writeNode(Node* node, Attributes attributes);


}




