
#pragma once

#include <string>
#include <assert.h>
#include <filesystem>

#include "Points.h"
#include "Vector3.h"

using std::string;
namespace fs = std::experimental::filesystem;

struct Cell {

	uint32_t count = 0;
	vector<Points*> batches;

};

class Chunker {
public:

	vector<Points*> batchesToDo;
	int32_t gridSize = 1; 
	string targetDirectory = "";

	vector<Cell> cells;

	Vector3<double> min = {0.0, 0.0, 0.0};
	Vector3<double> max = {0.0, 0.0, 0.0};

	Chunker(string targetDirectory, int gridSize) {
		this->targetDirectory = targetDirectory;
		this->gridSize = gridSize;

		cells.resize(gridSize * gridSize * gridSize);
	}

	void add(Points* batch) {

		double gridSizeD = double(gridSize);
		Vector3<double> size = max - min;
		Vector3<double> cellsD = Vector3<double>(gridSizeD, gridSizeD, gridSizeD);

		// for each cell, count how many points will be added
		int64_t numPoints = batch->points.size();
		vector<int> cells_numNew(gridSize * gridSize * gridSize);
		for (int64_t i = 0; i < numPoints; i++) {

			Point point = batch->points[i];
			double x = point.x;
			double y = point.y;
			double z = point.z;

			int32_t ux = int32_t(cellsD.x * (x - min.x) / size.x);
			int32_t uy = int32_t(cellsD.y * (y - min.y) / size.y);
			int32_t uz = int32_t(cellsD.z * (z - min.z) / size.z);

			ux = std::min(ux, gridSize - 1);
			uy = std::min(uy, gridSize - 1);
			uz = std::min(uz, gridSize - 1);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			assert(index < cells.size());

			cells_numNew[index]++;
		}

		// allocate necessary space for each cell
		for (uint64_t i = 0; i < cells_numNew.size(); i++) {
			int numNew = cells_numNew[i];

			if (numNew == 0) {
				continue;
			}

			Attribute aColor;
			aColor.byteOffset = 0;
			aColor.bytes = 4;
			aColor.name = "color";

			Points* cellBatch = new Points();
			cellBatch->attributeBuffer = new Buffer(numNew * aColor.bytes);
			cellBatch->attributes.push_back(aColor);

			cells[i].batches.push_back(cellBatch);
		}

		// now add them
		for(int64_t i = 0; i < batch->points.size(); i++) {
			Point point = batch->points[i];
			double x = point.x;
			double y = point.y;
			double z = point.z;

			int32_t ux = int32_t(cellsD.x * (x - min.x) / size.x);
			int32_t uy = int32_t(cellsD.y * (y - min.y) / size.y);
			int32_t uz = int32_t(cellsD.z * (z - min.z) / size.z);

			ux = std::min(ux, gridSize - 1);
			uy = std::min(uy, gridSize - 1);
			uz = std::min(uz, gridSize - 1);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			Cell& cell = cells[index];

			Points* cellBatch = cell.batches.back();

			uint8_t r = batch->attributeBuffer->dataU8[4 * i + 0];
			uint8_t g = batch->attributeBuffer->dataU8[4 * i + 1];
			uint8_t b = batch->attributeBuffer->dataU8[4 * i + 2];

			int64_t offset = cellBatch->points.size();
			cellBatch->points.push_back(point);

			uint8_t* rgbBuffer = cellBatch->attributeBuffer->dataU8 + (4 * offset + 0);

			rgbBuffer[0] = r;
			rgbBuffer[1] = g;
			rgbBuffer[2] = b;
			cell.count++;
		}

	}

};