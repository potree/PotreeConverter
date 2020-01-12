
#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "Vector3.h"
#include "Points.h"

using namespace std;


struct Cell {
	vector<Point> accepted;
};

class SparseGrid {

public:

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	double spacing = 1.0;
	double squaredSpacing = 1.0;

#define SP_MAP
//#define SP_VEC


#if defined(SP_MAP)
	unordered_map<uint64_t, Cell*> grid;
#elif defined(SP_VEC)
	vector<Cell*> grid;
#endif
	int64_t gridSize = 1;
	double gridSizeD = 1.0;

	SparseGrid(Vector3<double> min, Vector3<double> max, double spacing) {
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->spacing = spacing;
		this->squaredSpacing = spacing * spacing;

		gridSize = (size.x / spacing) / 1.0;
		gridSizeD = double(gridSize);

#if defined(SP_VEC)
		grid.resize(gridSize * gridSize * gridSize, nullptr);
#endif
	}

	bool isDistant(Point& candidate) {

		Vector3<int64_t> gridCoord = toGridCoordinate(candidate);

		int64_t xStart = std::max(gridCoord.x - 1, 0ll);
		int64_t yStart = std::max(gridCoord.y - 1, 0ll);
		int64_t zStart = std::max(gridCoord.z - 1, 0ll);
		int64_t xEnd = std::min(gridCoord.x + 1, gridSize - 1);
		int64_t yEnd = std::min(gridCoord.y + 1, gridSize - 1);
		int64_t zEnd = std::min(gridCoord.z + 1, gridSize - 1);

		for (uint64_t x = xStart; x <= xEnd; x++) {
			for (uint64_t y = yStart; y <= yEnd; y++) {
				for (uint64_t z = zStart; z <= zEnd; z++) {

					uint64_t index = x + y * gridSize + z * gridSize * gridSize;

#if defined(SP_VEC)
					auto cell = grid[index];

					if (cell == nullptr) {
						continue;
					}
#elif defined(SP_MAP)

					auto it = grid.find(index);
					if (it == grid.end()) {
						continue;
					}

					auto cell = it->second;
#endif

					for (Point& alreadyAccepted : cell->accepted) {
						double dd = candidate.squaredDistanceTo(alreadyAccepted);

						if (dd < squaredSpacing) {
							return false;
						}
					}

				}
			}
		}

		return true;
	}

	void add(Point& point) {
		uint64_t index = toGridIndex(point);

#if defined(SP_VEC)
		auto cell = grid[index];

		if (cell == nullptr) {
			cell = new Cell();

			grid[index] = cell;
		}

		cell->accepted.push_back(point);
#elif defined(SP_MAP)

		if (grid.find(index) == grid.end()) {
			Cell* cell = new Cell();

			grid.insert(std::make_pair(index, cell));
		}

		grid[index]->accepted.push_back(point);
#endif
	}

	Vector3<int64_t> toGridCoordinate(Point& point) {

		double x = point.x;
		double y = point.y;
		double z = point.z;

		int64_t ux = int32_t(gridSizeD * (x - min.x) / size.x);
		int64_t uy = int32_t(gridSizeD * (y - min.y) / size.y);
		int64_t uz = int32_t(gridSizeD * (z - min.z) / size.z);

		return { ux, uy, uz };
	}

	uint64_t toGridIndex(Point& point) {

		double x = point.x;
		double y = point.y;
		double z = point.z;

		int64_t ux = int32_t(gridSizeD * (x - min.x) / size.x);
		int64_t uy = int32_t(gridSizeD * (y - min.y) / size.y);
		int64_t uz = int32_t(gridSizeD * (z - min.z) / size.z);

		ux = std::min(ux, gridSize - 1);
		uy = std::min(uy, gridSize - 1);
		uz = std::min(uz, gridSize - 1);

		uint64_t index = ux + gridSize * uy + gridSize * gridSize * uz;

		return index;
	}

};