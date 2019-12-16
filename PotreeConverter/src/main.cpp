
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include <fstream>

#include <filesystem>

#include "LASLoader.hpp"

using namespace std::experimental;


namespace fs = std::experimental::filesystem;


future<void> run() {

	auto tStart = now();

	string path = "D:/dev/pointclouds/mschuetz/lion.las";
	//string path = "D:/dev/pointclouds/Riegl/Retz_Airborne_Terrestrial_Combined_1cm.las";
	LASLoader* loader = new LASLoader(path);

	
	Points* batch = co_await loader->nextBatch();
	while (batch != nullptr) {
		cout << "batch loaded" << endl;

		batch = co_await loader->nextBatch();
	}

	auto tEnd = now();
	auto duration = tEnd - tStart;
	cout << "duration: " << duration << endl;

}

int main(int argc, char **argv){

	run().wait();
	
	return 0;
}

