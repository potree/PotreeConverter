

#ifndef POTREEWRITER_LBL_H
#define POTREEWRITER_LBL_H

#include <string>

#include "boost/filesystem.hpp"

#include "AABB.h"
#include "CloudJS.hpp"
#include "SparseGrid.h"
#include "LASPointWriter.hpp"
#include "stuff.h"


using std::string;

namespace fs = boost::filesystem;


class PotreeWriterLBL{
public:
	AABB aabb;
	string path;
	float spacing;
	int maxLevel;
	long long numAccepted;
	long long numRejected;
	CloudJS cloudjs;
	SparseGrid *rootGrid;
	LASPointWriter *rWriter;
	LASPointWriter *dWriter;
	static const int chunkLimit = 100*1000*1000;
	bool partitioned;

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

	PotreeWriterLBL(string path, AABB aabb, float spacing, int maxLevel){
		this->path = path;
		this->aabb = aabb;
		this->spacing  = spacing;
		this->maxLevel = maxLevel;
		this->rootGrid = new SparseGrid(aabb, spacing);
		this->partitioned = false;
		numAccepted = 0;
		numRejected = 0;
		cloudjs.boundingBox = aabb;
		cloudjs.octreeDir = "data";
		cloudjs.outputFormat = OutputFormat::LAZ;
		cloudjs.spacing = spacing;
		cloudjs.version = "1.3";

		LASheader header;
		header.clean();
		header.point_data_format = 2;
		header.point_data_record_length = 26;
		header.set_bounding_box(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
		header.x_scale_factor = 0.01;
		header.y_scale_factor = 0.01;
		header.z_scale_factor = 0.01;
		header.x_offset = 0.0f;
		header.y_offset = 0.0f;
		header.z_offset = 0.0f;

		fs::remove_all(fs::path(path + "/data"));
		fs::remove_all(fs::path(path + "/temp"));

		string rPath = path + "/data/r.laz";
		string dPath = path + "/temp/d/d_0.laz";
		fs::create_directories(path + "/data");
		fs::create_directories(path + "/temp/d");



		rWriter = new LASPointWriter(rPath, header);
		dWriter = new LASPointWriter(dPath, header);
	}

	~PotreeWriterLBL(){
		close();
	}

	void add(Point &p){
		bool accepted = rootGrid->add(p.position());

		if(accepted){
			rWriter->write(p);
			numAccepted++;
		}else{
			dWriter->write(p);
			numRejected++;
		}

		if(numRejected  > 0 && (numRejected % chunkLimit) == 0){
			dWriter->close();
			delete dWriter;

			LASheader header;
			header.clean();
			header.point_data_format = 2;
			header.point_data_record_length = 26;
			header.set_bounding_box(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
			header.x_scale_factor = 0.01;
			header.y_scale_factor = 0.01;
			header.z_scale_factor = 0.01;
			header.x_offset = 0.0f;
			header.y_offset = 0.0f;
			header.z_offset = 0.0f;

			stringstream ssDPath;
			ssDPath << path << "/temp/d/d_" << numFilesInDirectory(path + "/temp/d") << ".laz";
			
			dWriter = new LASPointWriter(ssDPath.str(), header);
		}
	}

	/**
	 * from: http://stackoverflow.com/questions/6050298/how-do-i-count-the-number-of-files-in-a-directory-using-boostfilesystem
	 */
	int numFilesInDirectory(string dir){
		int count = 0;
		for(fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it){
			if(fs::is_regular_file(it->path())){
				count++;
			}
		} 

		return count;
	}

	void flush(){

	}

	void partition(){
		cloudjs.hierarchy.push_back(CloudJS::Node("r", numAccepted));

		partitioned = true;
		int depth = 1;
		string source = path + "/temp/d";
		string target = path + "/data/r";
		vector<Task> work;
		work.push_back(Task(source, target, aabb));

		while(!work.empty() && depth <= maxLevel){
			vector<Task> nextRound;
			
			vector<Task>::iterator it;
			for(it = work.begin(); it != work.end(); it++){
				source = it->source;
				target = it->target;
				AABB aabb = it->aabb;

				vector<int> indices = process(source, target, aabb, depth);

				// prepare the workload of the next level
				for(int i = 0; i < indices.size(); i++){
					int index = indices[i];
					stringstream ssSource, ssTarget;
					ssSource << source << indices[i];
					ssTarget << target << indices[i];
					AABB chAABB = childAABB(aabb, index);
					Task task(ssSource.str(), ssTarget.str(), chAABB);
					nextRound.push_back(task);
				}
			}

			depth++;
			work = nextRound;
		}

	}

	vector<int> process(string source, string target, AABB aabb, int depth){
		cout << "processing: " << source << endl;

		vector<int> indices;
		SparseGrid grid(aabb, spacing / pow(2.0, depth));

		{// add ancestors to the grid
			int numAncestors = target.size() - target.find_last_of("r");
			string ancestor = target;
			
			for(int i = 0; i < numAncestors; i++){
				LASPointReader *reader = new LASPointReader(ancestor + ".laz");
				while(reader->readNextPoint()){
					Point p = reader->getPoint();
					grid.addWithoutCheck(p.position());
				}

				ancestor = ancestor.substr(0, ancestor.size()-1);
			}

		}

		{// add source to grid
			LASPointReader *reader = new LASPointReader(source);

			// create writers for all 8 child nodes
			vector<LASPointWriter*> rWriters;
			vector<LASPointWriter*> dWriters;
			vector<int> numPoints;
			vector<int> numPointsRejected;
			for(int i = 0; i < 8; i++){
				stringstream ssr, ssd, ssdPath;
				ssr << target << i << ".laz";
				ssdPath << source << i;
				ssd << source << i << "/" << fs::path(source).filename().string() << i << "_0.laz";

				fs::create_directories(ssdPath.str());

				AABB cAABB = childAABB(aabb, i);
				LASheader header;
				header.clean();
				header.point_data_format = 2;
				header.point_data_record_length = 26;
				header.set_bounding_box(cAABB.min.x, cAABB.min.y, cAABB.min.z, cAABB.max.x, cAABB.max.y, cAABB.max.z);
				header.x_scale_factor = 0.01;
				header.y_scale_factor = 0.01;
				header.z_scale_factor = 0.01;
				header.x_offset = 0.0f;
				header.y_offset = 0.0f;
				header.z_offset = 0.0f;

				rWriters.push_back(new LASPointWriter(ssr.str(), header));
				dWriters.push_back(new LASPointWriter(ssd.str(), header));
				numPoints.push_back(0);
				numPointsRejected.push_back(0);
			}

			// write points
			while(reader->readNextPoint()){
				Point p = reader->getPoint();
				bool accepted = grid.add(p.position());
				int index = nodeIndex(aabb, p);
				if(index == -1){
					// should write warning
					continue;
				}

				if(accepted){
					rWriters[index]->write(p);
					if(find(indices.begin(), indices.end(), index) == indices.end()){
						indices.push_back(index);
					}
					numPoints[index]++;
					numAccepted++;
				}else{
					dWriters[index]->write(p);
					numPointsRejected[index]++;

					if(numPointsRejected[index]  > 0 && (numPointsRejected[index] % chunkLimit) == 0){
						LASheader header(*dWriters[index]->header);
						stringstream ssDPath, ssDFile;
						ssDPath << source << index;
						ssDFile << source << index << "/" << fs::path(source).filename() << index << "_" << numFilesInDirectory(ssDPath.str()) << ".laz";

						dWriters[index]->close();
						delete dWriters[index];
			
						dWriters[index] = new LASPointWriter(ssDFile.str(), header);
					}
				}
			}

			// cleanup
			for(int i = 0; i < 8; i++){
				rWriters[i]->close();
				dWriters[i]->close();

				// remove unnecessary r files. 
				if(numPointsRejected[i] == 0 && numPoints[i] == 0){
					fs::remove(rWriters[i]->file);
				}
				
				delete rWriters[i];
				delete dWriters[i];
			}

			// update cloud.js
			{
				for(int i = 0; i < 8; i++){
					if(numPoints[i] > 0 || numPointsRejected[i] > 0){
						stringstream ssNodeName;
						ssNodeName << fs::path(target).filename().string() << i;
						cloudjs.hierarchy.push_back(CloudJS::Node(ssNodeName.str(), numPoints[i]));
					}

					saveCloudJS();
				}
			}
		}

		return indices;
	}

	void saveCloudJS(){
		ofstream oCloudJS(path + "/cloud.js", ios::out);
		oCloudJS << cloudjs.string();
		oCloudJS.close();
	}

	void close(){
		if(rWriter != NULL){
			rWriter->close();
			delete rWriter;
			rWriter = NULL;
		}

		if(dWriter != NULL){
			dWriter->close();
			delete dWriter;
			dWriter = NULL;
		}

		if(rootGrid != NULL){
			delete rootGrid;
			rootGrid = NULL;
		}

		if(!partitioned){
			partition();
		}

		saveCloudJS();
	}

};


#endif

