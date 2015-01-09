


#ifndef POTREEWRITER_TILING1_H
#define POTREEWRITER_TILING1_H

#include <chrono>
#include <string>
#include <sstream>
#include <list>
#include <map>
#include <vector>

#include "boost/filesystem.hpp"

#include "AABB.h"
#include "LASPointWriter.hpp"
#include "LASPointReader.h"
#include "SparseGrid.h"
#include "stuff.h"
#include "CloudJS.hpp"
#include "PointReader.h"
#include "PointWriter.hpp"



using std::string;
using std::stringstream;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;


struct MortonPoint{
	Point p;
	unsigned int code;
	int level;

	MortonPoint(Point p, unsigned int code){
		this->p = p;
		this->code = code;
		this->level = 0;
	}
};

bool mortonOrder(MortonPoint a, MortonPoint b){ return a.code < b.code; }

class PotreeWriterTiling1{

public:

	AABB aabb;
	AABB tightAABB;
	string path;
	float spacing;
	int maxLevel;
	CloudJS cloudjs;
	int numAccepted;

	vector<MortonPoint> cache;
	


	PotreeWriterTiling1(string path, AABB aabb, float spacing, int maxLevel, double scale, OutputFormat outputFormat){
		this->path = path;
		this->aabb = aabb;
		this->spacing = spacing;
		this->maxLevel = maxLevel;

		fs::remove_all(path + "/data");
		fs::remove_all(path + "/temp");
		fs::create_directories(path + "/data");
		fs::create_directories(path + "/temp");
		

		cloudjs.outputFormat = outputFormat;
		cloudjs.boundingBox = aabb;
		cloudjs.octreeDir = "data";
		cloudjs.spacing = spacing;
		cloudjs.version = "1.4";
		cloudjs.scale = scale;



	}

	unsigned int mortonCode(Point p){
		unsigned int mx = 255 * (p.x - aabb.min.x) / (aabb.max.x - aabb.min.x);
		unsigned int my = 255 * (p.y - aabb.min.y) / (aabb.max.y - aabb.min.y);
		unsigned int mz = 255 * (p.z - aabb.min.z) / (aabb.max.z - aabb.min.z);

		unsigned int mc = 0;
		for(int i = 0; i < 8; i++){
			int mask = pow(2, i);
			mc = mc | (mx & mask) << (3 * i + 0 - i);
			mc = mc | (my & mask) << (3 * i + 1 - i);
			mc = mc | (mz & mask) << (3 * i + 2 - i);
		}

		return mc;
	}


	void add(Point &p){

		MortonPoint mp(p, mortonCode(p));
		cache.push_back(mp);

	}

	

	void flush(){

		//sort(cache.begin(), cache.end(), mortonOrder);

		static int c = 0;

		int maxLevel = 11;

		map<unsigned int, LASPointWriter*> writers;

		auto startSort = high_resolution_clock::now();
		float gs = ( 1.0f - pow(4.0f, (float)maxLevel + 1.0f) ) / (1.0f - 4.0f);
		vector<float> props;
		for(int i = 0; i <= maxLevel; i++){
			float p = pow(4.0f, (float)i) / gs;

			if(i > 0){
				p = p + props[i-1];
			}

			props.push_back(p);
		}


		for(int i = 0; i < cache.size(); i++){

			MortonPoint &mp = cache[i];

			float r = (float)rand() / RAND_MAX;
			for(int j = 0; j <= maxLevel; j++){
				if(r < props[j]){
					mp.level = j;
					break;
				}
			}


			unsigned int mc = mp.code;
			mc = mc >> (3 * (8 - mp.level) );
			mc = mc << (3 * (8 - mp.level) );
			if(mp.level == 0){
				mc = -1;
			}
			mp.code = mc;
		}
		sort(cache.begin(), cache.end(), mortonOrder);
		
		auto endSort = high_resolution_clock::now();
		milliseconds duration = duration_cast<milliseconds>(endSort-startSort);
		cout << "sort: " << (duration.count()/1000.0f) << "s" << endl;

		auto startWrite = high_resolution_clock::now();

		int prevMc = -2;
		LASPointWriter *writer = NULL;
		for(int i = 0; i < cache.size(); i++){
			MortonPoint &mp = cache[i];
			unsigned int mc = mp.code;

			if(writer == NULL || mc != prevMc){

				if(writer != NULL){
					writer->close();
					delete writer;
				}

				stringstream ssFile;
				ssFile << "D:/temp/tiling/r";
			
				for(int j = 0; j < mp.level; j++){
					int index = (mc >> ( 3 * (8-j) )) & 7;
					ssFile << index;
				}
				ssFile << ".las";
			
				writer = new LASPointWriter(ssFile.str(), aabb, 0.001);
			}

			Point p = cache[i].p;
			writer->write(mp.p);

			prevMc = mc;

		}
		if(writer != NULL){
			writer->close();
			delete writer;
		}

		auto endWrite = high_resolution_clock::now();
		duration = duration_cast<milliseconds>(endWrite-startWrite);
		cout << "write: " << (duration.count()/1000.0f) << "s" << endl;


		cache.clear();
		c++;

	}

	void close(){
		flush();




	}




};


#endif