

#include <string>

//#include <filesystem>

#include "stuff.h"

using std::string;

//using std::string;

#include "Chunker.h"
#include "Indexer_Centered.h"
#include "./modules/index_bluenoise/Indexer.h"
#include "Indexer_Centered_countsort.h"
#include "Indexer_Centered_nochunks.h"

#include "LASLoader.hpp"


void tests() {
	//{
	//	Attributes attributes = estimateAttributes(pathIn);

	//	printElapsedTime("estimate attributes", tStart);

	//	for (auto attribute : attributes.list) {
	//		cout << attribute.name << endl;
	//	}

	//	cout << "size: " << attributes.byteSize << endl;
	//}

	//{
	//	//string in = "D:/dev/pointclouds/mschuetz/lion.las";
	//	string in = "D:/dev/pointclouds/open_topography/ca13/morro_rock/ot_35120C7101A_1.laz";
	//	LASLoader loader(in, 1);

	//	auto promise = loader.nextBatch();
	//	promise.wait();
	//	auto batch = promise.get();

	//	writeLAS("D:/temp/test/test.las", batch);

	//	//while (batch != nullptr) {

	//	//	static int i = 0;
	//	//	string file = "D:/temp/test/test_" + to_string(i) + ".las";
	//	//	writeLAS(file, batch);
	//	//	i++;

	//	//	promise = loader.nextBatch();
	//	//	promise.wait();
	//	//	batch = promise.get();
	//	//}
	//}
}

int main(int argc, char **argv){


	double tStart = now();

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/Riegl/niederweiden.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/open_topography/ca13/morro_rock/merged.las";
	
	string pathIn = "D:/dev/pointclouds/pix4d/eclepens.las";
	//string pathIn = "D:/dev/pointclouds/archpro/heidentor.las";
	//string pathIn = "D:/dev/pointclouds/mschuetz/lion.las";
	//string pathIn = "D:/dev/pointclouds/mschuetz/plane.las";
	//string pathIn = "D:/dev/pointclouds/mschuetz/plane_small.las";

	string pathOut = "C:/dev/workspaces/potree/develop/test/converter1/pointcloud";

	//doChunking(pathIn, pathOut);

	//countsort::doIndexing(pathOut);
	//centered::doIndexing(pathOut);
	//centered_nochunks::doIndexing(pathIn, pathOut);
	//bluenoise::doIndexing(pathOut);

	printElapsedTime("total time", tStart);


	return 0;
}

