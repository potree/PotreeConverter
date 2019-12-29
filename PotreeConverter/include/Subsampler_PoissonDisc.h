
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

#include "Points.h"
#include "Vector3.h"

using std::string;
using std::cout;
using std::endl;
using std::numeric_limits;
using std::vector;
using std::unordered_map;
namespace fs = std::experimental::filesystem;



class Subsampler_PoissonDisc {

	struct Cell {
		Vector3<double> point;
		double squaredDistToCellCenter = numeric_limits<double>::infinity();
	};


public:

	string targetDirectory = "";
	double spacing = 1.0;
	double spacingSquared = 1.0;
	Vector3<double> min = { 0.0, 0.0, 0.0 };
	Vector3<double> max = { 0.0, 0.0, 0.0 };
	Vector3<double> size = { 0.0, 0.0, 0.0 };
	double cubeSize = 0.0;

	vector<Cell*> grid;
	//unordered_map<int64_t, Cell*> grid;
	vector<Vector3<double>> accepted;
	int gridSize = 1.0;
	vector<int64_t> randomIndices;

	Subsampler_PoissonDisc(string targetDirectory, double spacing, Vector3<double> min, Vector3<double> max) {
		this->targetDirectory = targetDirectory;
		this->spacing = spacing;
		this->min = min;
		this->max = max;
		this->size = max - min;
		this->cubeSize = std::max(std::max(size.x, size.y), size.z);

		this->spacingSquared = spacing * spacing;

		double cellSize = sqrt(2.0 * spacingSquared);
		this->gridSize = int(cubeSize / cellSize);

		cout << "gridSize: " << gridSize << endl;
		// vector variant
		int numCells = gridSize * gridSize * gridSize;
		grid.resize(numCells, nullptr);

		randomIndices.reserve(1'000'000);
		for (int64_t i = 0; i < 1'000'000; i++) {
			randomIndices.push_back(i);
		}
		std::random_shuffle(randomIndices.begin(), randomIndices.end());
	}

	double closestDistanceToNeighbor(
		double x, double y, double z, 
		int32_t ux, int32_t uy, int32_t uz) {

		//int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

		double closestDistance = 10000000.0;

		int64_t xStart = std::max(ux - 2, 0);
		int64_t yStart = std::max(uy - 2, 0);
		int64_t zStart = std::max(uz - 2, 0);
		int64_t xEnd = std::min(ux + 2, gridSize - 1);
		int64_t yEnd = std::min(uy + 2, gridSize - 1);
		int64_t zEnd = std::min(uz + 2, gridSize - 1);

		for (int64_t nx = xStart; nx <= xEnd; nx++) {
		for (int64_t ny = yStart; ny <= yEnd; ny++) {
		for (int64_t nz = zStart; nz <= zEnd; nz++) {
			
			int32_t index = nx + gridSize * ny + gridSize * gridSize * nz;

			// map variant
			//if (grid.find(index) == grid.end()) {
			//	continue;
			//}

			Cell* cell = grid[index];

			// vector variant
			if (cell == nullptr) { 
				continue;
			}

			double dx = cell->point.x - x;
			double dy = cell->point.y - y;
			double dz = cell->point.z - z;

			double dd = dx * dx + dy * dy + dz * dz;

			closestDistance = std::min(closestDistance, dd);

			if (closestDistance <= spacingSquared) {
				return closestDistance;
			}

		}
		}
		}

		return closestDistance;
	}

	void add(Points* batch) {

		double gridSizeD = double(gridSize);
		Vector3<double> cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);

		


		//for (int64_t i = 0; i < batch->count; i++) {
		for (int64_t iii = 0; iii < batch->points.size(); iii++) {

			int64_t i = iii;
			if (batch->points.size() == randomIndices.size()) {
				i = randomIndices[iii];
			}

			Point point = batch->points[i];
			double x = point.x;
			double y = point.y;
			double z = point.z;

			double gx = cellsD.x * (x - min.x) / size.x;
			double gy = cellsD.y * (y - min.y) / size.y;
			double gz = cellsD.z * (z - min.z) / size.z;

			gx = std::min(gx, std::nextafter(gx, min.x));
			gy = std::min(gy, std::nextafter(gy, min.y));
			gz = std::min(gz, std::nextafter(gz, min.z));

			int32_t ux = int32_t(gx);
			int32_t uy = int32_t(gy);
			int32_t uz = int32_t(gz);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			// vector variant
			if (grid[index] != nullptr) {
				continue;
			}

			// map variant
			//if (grid.find(index) != grid.end()) {
			//	continue;
			//}

			double closestSquared = closestDistanceToNeighbor(x, y, z, ux, uy, uz);

			if (closestSquared > spacingSquared) {
				Cell* cell = new Cell();
				cell->point = { x, y, z };
				grid[index] = cell;
			}

		}
	}

	vector<Vector3<double>> getPoints() {

		vector<Vector3<double>> points;

		// map variant
		//for (auto it : grid) {

		//	auto point = it.second->point;
		//	points.push_back(point);
		//}

		// vector variant
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