
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
	Point accepted;
	int location = 0;
	int count = 0;
};

class SparseGrid {

public:

	Vector3<double> min;
	Vector3<double> max;
	Vector3<double> size;
	double spacing = 1.0;
	double squaredSpacing = 1.0;

	//vector<Cell*> grid;
	unordered_map<uint64_t, Cell> grid;
	vector<Point> accepted;

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

		//grid.resize(gridSize * gridSize * gridSize, nullptr);
	}

	enum class Actions {
		add,
		exchange,
		pass,
	};

	struct Action {
		Actions action;
		Point& subject;
	};

	Action evaluate(Point& candidate) {
		uint64_t index = toGridIndex(candidate);

		Action action = {Actions::add, candidate};

		auto it = grid.find(index);
		if (it == grid.end()) {
			grid[index] = Cell();

			it = grid.find(index);
		}
		Cell& cell = it->second;

		cell.count++;

		if (cell.count == 1) {
			action.action = Actions::add;
		} else {
			action.action = Actions::pass;
		}

		if (action.action == Actions::add) {
			cell.accepted = candidate;

			accepted.push_back(candidate);
			cell.location = accepted.size() - 1;

		} else if (action.action == Actions::exchange) {
			Point oldPoint = cell.accepted;
			cell.accepted = candidate;
			action.subject = oldPoint;
		} else if (action.action == Actions::pass) {

		}

		return action;
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