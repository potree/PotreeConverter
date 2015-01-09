


#ifndef POTREEWRITER_TILING_H
#define POTREEWRITER_TILING_H

#include <string>
#include <sstream>
#include <list>

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

namespace fs = boost::filesystem;

class PotreeWriterTiling{

public:

	AABB aabb;
	AABB tightAABB;
	string path;
	float spacing;
	int maxLevel;
	CloudJS cloudjs;
	int numAccepted;

	vector<LASPointWriter*> writers;


	int splitDepth;
	int splitDepthCoefficient;

	Vector3<double> prevPos;


	PotreeWriterTiling(string path, AABB aabb, float spacing, int maxLevel, double scale, OutputFormat outputFormat){
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



		splitDepth = 3;
		splitDepthCoefficient = pow(2, splitDepth);
		writers.resize(pow(8, splitDepth), NULL);
	
	}


	void add(Point &p){


		Vector3<double> pos = p.position();
		Vector3<double> dipos = ((pos - aabb.min) / aabb.size) * (double)splitDepthCoefficient;
		Vector3<unsigned int> iPos(dipos.x, dipos.y, dipos.z);
		
		int mc = 0;
		for(int i = 0; i < splitDepth; i++){
			int mask = pow(2, i);
			mc = mc | (iPos.x & mask) << (3 * i + 0 - i);
			mc = mc | (iPos.y & mask) << (3 * i + 1 - i);
			mc = mc | (iPos.z & mask) << (3 * i + 2 - i);
		}
		
		if(writers[mc] == NULL){
			stringstream ssFilename;
			ssFilename << path << "/temp/r";
			for(int i = 0; i < splitDepth; i++){
				int mask = pow(2, i);
				int index = 0;
				index = index | ((iPos.x & mask) >> i) << 0;
				index = index | ((iPos.y & mask) >> i) << 1;
				index = index | ((iPos.z & mask) >> i) << 2;
		
				ssFilename << index;
			}
			ssFilename << ".las";
		
		
			double scale = 0.001;
			writers[mc] = new LASPointWriter(ssFilename.str(), aabb, scale);
		}
		
		LASPointWriter *writer = writers[mc];
		
		writer->write(p);
	}

	void flush(){

	}

	class Task{
	public:

		int splitDepth;
		string path;

		Task(string path, int splitDepth){
			this->path = path;
			this->splitDepth = splitDepth;
		}
	};

	void close(){
		flush();

		vector<Task> remainingTasks;
		for(int i = 0; i < pow(8, splitDepth); i++){
			if(writers[i] != NULL){
				
				if(writers[i]->numPoints > 10*1000*1000){
					Task task(writers[i]->file, splitDepth + 3);
					remainingTasks.push_back(task);
				}
				writers[i]->close();

				delete writers[i];
				writers[i] = NULL;
			}
		}

		int splitOffset = splitDepth;
		while(!remainingTasks.empty()){

			splitDepthCoefficient = pow(2, splitOffset + splitDepth);

			for(int i = 0; i < remainingTasks.size(); i++){
				Task task = remainingTasks[i];
				cout << "continue splitting: " << task.path << endl;

				LASPointReader *reader = new LASPointReader(task.path);
				string path = task.path.substr(0, task.path.find_last_of("."));

				while(reader->readNextPoint()){
					Point p = reader->getPoint();

					Vector3<double> pos = p.position();
					Vector3<double> dipos = ((pos - aabb.min) / aabb.size) * (double)splitDepthCoefficient;
					Vector3<unsigned int> iPos(dipos.x, dipos.y, dipos.z);
		
					int mc = 0;
					for(int i = 0; i < splitOffset + splitDepth; i++){
						int mask = pow(2, i);
						mc = mc | (iPos.x & mask) << (3 * i + 0 - i);
						mc = mc | (iPos.y & mask) << (3 * i + 1 - i);
						mc = mc | (iPos.z & mask) << (3 * i + 2 - i);
					}
					//mc = mc >> (3*splitOffset);
					unsigned int mask = (1U << (splitDepth*3)) - 1U;
					mc = mc & mask;
		
					if(writers[mc] == NULL){
						stringstream ssFilename;
						ssFilename << path;
						for(int i = 0; i < splitDepth; i++){
							int mask = 7;
							int index = (mc >> (3*i)) & mask;
		
							ssFilename << index;
						}
						ssFilename << ".las";
		
		
						double scale = 0.001;
						writers[mc] = new LASPointWriter(ssFilename.str(), aabb, scale);
					}
		
					LASPointWriter *writer = writers[mc];
		
					writer->write(p);

				}
				reader->close();
				delete reader;


				boost::filesystem::remove(boost::filesystem::path(task.path));
			}

			remainingTasks.clear();
			for(int i = 0; i < pow(8, splitDepth); i++){
				if(writers[i] != NULL){
				
					if(writers[i]->numPoints > 10*1000*1000){
						Task task(writers[i]->file, splitDepth + 3);
						remainingTasks.push_back(task);
					}
					writers[i]->close();

					delete writers[i];
					writers[i] = NULL;
				}
			}

			splitOffset += splitDepth;

		}


		map<int, LASPointWriter*> writers;
		LASPointWriter *writer = new LASPointWriter(this->path + "/data/r.las", aabb, 0.001);
		fs::path p(this->path + "/temp");
		fs::directory_iterator end_itr;
		for (fs::directory_iterator itr(p); itr != end_itr; ++itr){
			 fs::path rFile = itr->path();

			 LASPointReader *reader = new LASPointReader(rFile.string());

			 while(reader->readNextPoint()){
				Point p = reader->getPoint();

				float r = (float)rand() / RAND_MAX;
				



				Vector3<double> pos = p.position();
				Vector3<double> dipos = ((pos - aabb.min) / aabb.size) * (double)splitDepthCoefficient;
				Vector3<unsigned int> iPos(dipos.x, dipos.y, dipos.z);
		
				int mc = 0;
				for(int i = 0; i < splitDepth; i++){
					int mask = pow(2, i);
					mc = mc | (iPos.x & mask) << (3 * i + 0 - i);
					mc = mc | (iPos.y & mask) << (3 * i + 1 - i);
					mc = mc | (iPos.z & mask) << (3 * i + 2 - i);
				}


				int level = 0;
				if(r < 0.02){
					level = 0;
					mc = -1;
				}else if(r < 0.08){
					level = 1;
				}else if(r < 0.32){
					level = 2;
				}else if(r < 0.5){
					level = 3;
				}else{
					continue;
				}

				mc = mc >> (splitDepth-level) << (splitDepth-level);

				if(writers.find(mc) == writers.end()){
					stringstream rPath;
					rPath << path << "/data/r";

					for(int i = 0; i < level; i++){
						int index = (mc >> (3*(splitDepth - i - 1))) & 7;
						rPath << index;
					}
					rPath << ".las";

					LASPointWriter *writer = new LASPointWriter(rPath.str(), aabb, 0.001);
					writers[mc] = writer;
				}

				writers[mc]->write(p);




			}

			 reader->close();
			 delete reader;

		} 
		
		std::map< int, LASPointWriter* >::const_iterator iter;
		for(iter = writers.begin(); iter != writers.end(); ++iter ){
			iter->second->close();
			delete iter->second;
		}




	}




};


#endif