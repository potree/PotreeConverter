


#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "PotreeException.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "lasdefinitions.hpp"
#include "lasreader.hpp"
#include "laswriter.hpp"

#include "LASPointReader.h"
#include "LASPointWriter.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <math.h>
#include <fstream>

using std::stringstream;
using std::map;
using std::string;
using std::vector;
using std::find;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::fstream;
using boost::iends_with;
using boost::filesystem::is_directory;
using boost::filesystem::directory_iterator;
using boost::filesystem::is_regular_file;
using boost::filesystem::path;

struct Task{
	string source;
	string target;
	AABB aabb;

	Task(string source, string target, AABB aabb){
		this->source = source;
		this->target = target;
		this->aabb = aabb;
	}
};

PotreeConverter::PotreeConverter(vector<string> sources, string workDir, float spacing, int maxDepth, string format, float range, OutputFormat outFormat){

	// if sources contains directories, use files inside the directory instead
	vector<string> sourceFiles;
	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];
		path pSource(source);
		if(boost::filesystem::is_directory(pSource)){
			for(path::iterator it = pSource.begin(); it != pSource.end(); it++){
				path pDirectoryEntry = *it;
				if(boost::filesystem::is_regular_file(pDirectoryEntry)){
					sourceFiles.push_back(pDirectoryEntry.string());
				}
			}
		}else if(boost::filesystem::is_regular_file(pSource)){
			sourceFiles.push_back(source);
		}
	}
	

	this->sources = sourceFiles;
	this->workDir = workDir;
	this->spacing = spacing;
	this->maxDepth = maxDepth;
	this->format = format;
	this->range = range;
	this->outputFormat = outFormat;

	boost::filesystem::path dataDir(workDir + "/data");
	boost::filesystem::path tempDir(workDir + "/temp");
	boost::filesystem::create_directories(dataDir);
	boost::filesystem::create_directories(tempDir);
}


string PotreeConverter::getOutputExtension(){
	string outputExtension = "";
	if(outputFormat == OutputFormat::LAS){
		outputExtension = ".las";
	}else if(outputFormat == OutputFormat::LAZ){
		outputExtension = ".laz";
	}

	return outputExtension;
}

AABB calculateAABB(vector<string> sources){
	AABB aabb;

	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		LASPointReader *reader = new LASPointReader(source);
		LASheader header = reader->getHeader();

		Vector3 lmin = Vector3(header.min_x, header.min_y, header.min_z);
		Vector3 lmax = Vector3(header.max_x, header.max_y, header.max_z);

		aabb.update(lmin);
		aabb.update(lmax);

		delete reader;
	}

	return aabb;
}

class Node{
public:
	
	AABB aabb;
	string name;
	SparseGrid grid;
	float spacing;
	int level;
	int maxLevel;
	string path;
	LASPointWriter *writer;
	Node *children[8];
	int numAccepted;
	AABB acceptedAABB;


	Node(string name, string path, AABB aabb, float spacing, int level, int maxLevel)
		:grid(aabb, spacing)
	{
		this->spacing = spacing;
		this->name = name;
		this->level = level;
		this->maxLevel = maxLevel;
		this->path = path;
		this->aabb = aabb;
		numAccepted = 0;

		init();
	}

	~Node(){
		if(level < maxLevel){
			for(int i = 0; i < 8; i++){
				delete children[i];
			}
		}
	}

	void close(){


		removeEmpty();
	}

	void updateAABB(){
		Vector3 min = acceptedAABB.min;
		Vector3 max = acceptedAABB.max;
		writer->header.set_bounding_box(min.x, min.y, min.z, max.x, max.y, max.z);
		writer->writer->update_header(&writer->header, false);

		if(level < maxLevel){
			for(int i = 0; i < 8; i++){
				children[i]->updateAABB();
			}
		}
	}

	bool removeEmpty(){
		writer->close();
		delete writer;

		if(level == maxLevel){
			if(numAccepted == 0){
				boost::filesystem::remove(boost::filesystem::path(path + "/" + name + ".las"));

				return true;
			}else{
				return false;
			}
		}else{
			bool allRemoved = true;
			for(int i = 0; i < 8; i++){
				bool removed = children[i]->removeEmpty();
				allRemoved = allRemoved && removed;
			}

			if(allRemoved && numAccepted == 0){
				boost::filesystem::remove(boost::filesystem::path(path + "/" + name + ".las"));

				return true;
			}else{
				return false;
			}
		}
	}

	void init(){
		//aabb.makeCubic();

		LASheader header;
		header.clean();
		header.set_bounding_box(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
		header.x_scale_factor = 0.01;
		header.y_scale_factor = 0.01;
		header.z_scale_factor = 0.01;
		header.x_offset = 0.0f;
		header.y_offset = 0.0f;
		header.z_offset = 0.0f;

		writer = new LASPointWriter(path + "/" + name + ".las", header);

		if(level < maxLevel){
			for(int i = 0; i < 8; i++){
				stringstream ssName;
				ssName << name << i;
				children[i] = new Node(ssName.str(), path, childAABB(aabb, i), spacing / 2.0f, level+1, maxLevel);
			}
		}
	}

	void add(Point &p){
		bool accepted = grid.add(p);

		if(accepted){
			writer->write(p);
			acceptedAABB.update(Vector3(p.x, p.y, p.z));
			numAccepted++;
		}else if(level < maxLevel){
			int childIndex = nodeIndex(aabb, p);
			if(childIndex >= 0){
				children[childIndex]->add(p);
			}
		}
	}

};

void PotreeConverter::convert(){
	aabb = calculateAABB(sources);
	cout << "AABB: " << endl << aabb << endl;

	Node *root = new Node("r", "C:/temp/test/", aabb, spacing, 0, 2);

	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		LASPointReader *reader = new LASPointReader(source);
		while(reader->readNextPoint()){
			Point p = reader->getPoint();
			root->add(p);
		}
	}
	
	root->close();

	delete root;
}
