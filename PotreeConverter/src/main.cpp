

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


int main(int argc, char **argv){

	double tStart = now();

	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/Riegl/niederweiden.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	//string path = "D:/dev/pointclouds/open_topography/ca13/morro_rock/merged.las";
	
	 //string pathIn = "D:/dev/pointclouds/pix4d/eclepens.las";
	  //string pathIn = "D:/dev/pointclouds/archpro/heidentor.las";
	string pathIn = "D:/dev/pointclouds/mschuetz/lion.las";
	//string pathIn = "D:/dev/pointclouds/mschuetz/plane.las";
	//string pathIn = "D:/dev/pointclouds/mschuetz/plane_small.las";

	string pathOut = "D:/temp/test";

	doChunking(pathIn, pathOut);

	//countsort::doIndexing(pathOut);
	//centered::doIndexing(pathOut);
	//centered_nochunks::doIndexing(pathIn, pathOut);
	bluenoise::doIndexing(pathOut);

	printElapsedTime("total time", tStart);


	return 0;
}

