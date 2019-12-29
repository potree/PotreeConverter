
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>
#include <iostream>
#include <limits>

#include "Points.h"
#include "Vector3.h"

using std::string;
using std::cout;
using std::endl;
using std::numeric_limits;
namespace fs = std::experimental::filesystem;



class Subsampler {

	struct Cell {
		Vector3<double> point;
		double squaredDistToCellCenter = numeric_limits<double>::infinity();
	};


public:

	string targetDirectory = "";
	double spacing = 1.0;
	Vector3<double> min = { 0.0, 0.0, 0.0 };
	Vector3<double> max = { 0.0, 0.0, 0.0 };
	Vector3<double> size = { 0.0, 0.0, 0.0 };
	double cubeSize = 0.0;

	vector<Cell*> grid;
	vector<Vector3<double>> accepted;
	int gridSize = 1.0;

	Subsampler(string targetDirectory, double spacing, Vector3<double> min, Vector3<double> max) {
		this->targetDirectory = targetDirectory;
		this->spacing = spacing;
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->cubeSize = std::max(std::max(size.x, size.y), size.z);

		this->gridSize = int(cubeSize / spacing);

		cout << "gridSize: " << gridSize << endl;
		int numCells = gridSize * gridSize * gridSize;
		grid.resize(numCells, nullptr);

	}

	void add(Points* batch) {

		double gridSizeD = double(gridSize);
		Vector3<double> cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);

		for (int64_t i = 0; i < batch->count; i++) {
			double x = batch->data[0]->dataD[3 * i + 0];
			double y = batch->data[0]->dataD[3 * i + 1];
			double z = batch->data[0]->dataD[3 * i + 2];

			double gx = cellsD.x * (x - min.x) / size.x;
			double gy = cellsD.y * (y - min.y) / size.y;
			double gz = cellsD.z * (z - min.z) / size.z;

			gx = std::min(gx, std::nextafter(gx, min.x));
			gy = std::min(gy, std::nextafter(gy, min.y));
			gz = std::min(gz, std::nextafter(gz, min.z));

			double dx = fmod(gx, 1.0);
			double dy = fmod(gy, 1.0);
			double dz = fmod(gz, 1.0);
			double squaredDistToCellCenter = dx * dx + dy * dy + dz * dz;

			int32_t ux = int32_t(gx);
			int32_t uy = int32_t(gy);
			int32_t uz = int32_t(gz);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			assert(index >= 0 && index < grid.size());

			if (grid[index] == nullptr) {
				Cell* cell = new Cell();

				cell->point = { x, y, z };
				grid[index] = cell;
			} else {
				Cell* cell = grid[index];

				if (squaredDistToCellCenter < cell->squaredDistToCellCenter) {
					cell->point = {x, y, z};
					cell->squaredDistToCellCenter = squaredDistToCellCenter;
				}
			}
		}
	}

	vector<Vector3<double>> getPoints() {

		vector<Vector3<double>> points;

		for (Cell* cell : grid) {
			if (cell == nullptr) {
				continue;
			}

			auto point = cell->point;
			points.push_back(point);
		}

		return points;
	}

};