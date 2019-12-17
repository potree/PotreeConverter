
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
	int32_t gridSize = 32;
	string targetDirectory = "";

	vector<Cell> cells;

	Vector3<double> min = {0.0, 0.0, 0.0};
	Vector3<double> max = {0.0, 0.0, 0.0};

	double minX = 0.0;
	double minY = 0.0;
	double minZ = 0.0;
	double maxX = 0.0;
	double maxY = 0.0;
	double maxZ = 0.0;

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
		vector<int> cells_numNew(gridSize * gridSize * gridSize);
		for (int64_t i = 0; i < batch->count; i++) {

			double x = batch->attributes[0].data->dataD[3 * i + 0];
			double y = batch->attributes[0].data->dataD[3 * i + 1];
			double z = batch->attributes[0].data->dataD[3 * i + 2];

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

			Attribute aPosition;
			aPosition.byteOffset = 0;
			aPosition.bytes = 24;
			aPosition.name = "position";
			aPosition.data = new Buffer(numNew * aPosition.bytes);

			Attribute aColor;
			aColor.byteOffset = 12;
			aColor.bytes = 4;
			aColor.name = "color";
			aColor.data = new Buffer(numNew * aColor.bytes);

			Points* cellBatch = new Points();
			cellBatch->count = 0;
			cellBatch->attributes.push_back(aPosition);
			cellBatch->attributes.push_back(aColor);

			cells[i].batches.push_back(cellBatch);
		}

		// now add them
		for(int64_t i = 0; i < batch->count; i++) {
			double x = batch->attributes[0].data->dataD[3 * i + 0];
			double y = batch->attributes[0].data->dataD[3 * i + 1];
			double z = batch->attributes[0].data->dataD[3 * i + 2];

			int32_t ux = int32_t(cellsD.x * (x - min.x) / size.x);
			int32_t uy = int32_t(cellsD.y * (y - min.y) / size.y);
			int32_t uz = int32_t(cellsD.z * (z - min.z) / size.z);

			ux = std::min(ux, gridSize - 1);
			uy = std::min(uy, gridSize - 1);
			uz = std::min(uz, gridSize - 1);

			int32_t index = ux + gridSize * uy + gridSize * gridSize * uz;

			Cell& cell = cells[index];
			Points* cellBatch = cell.batches.back();

			uint8_t r = batch->attributes[1].data->dataU8[4 * i + 0];
			uint8_t g = batch->attributes[1].data->dataU8[4 * i + 1];
			uint8_t b = batch->attributes[1].data->dataU8[4 * i + 2];

			int64_t offset = cellBatch->count;
			cellBatch->attributes[0].data->dataD[3 * offset + 0] = x;
			cellBatch->attributes[0].data->dataD[3 * offset + 1] = y;
			cellBatch->attributes[0].data->dataD[3 * offset + 2] = z;

			cellBatch->attributes[1].data->dataU8[4 * offset + 0] = r;
			cellBatch->attributes[1].data->dataU8[4 * offset + 1] = g;
			cellBatch->attributes[1].data->dataU8[4 * offset + 2] = b;
			//cellBatch->attributes[1].data->dataU8[4 * offset + 3] = 255;
			cellBatch->count++;
			cell.count++;
		}



		//int count = 0;
		//int sum = 0;
		//for (int i = 0; i < cells_numNew.size(); i++) {
		//	if (cells_numNew[i] > 0) {
		//		count++;
		//	}

		//	sum += cells_numNew[i];
		//}

		//cout << "count: " << count << ", sum: " << sum << endl;

	}

};