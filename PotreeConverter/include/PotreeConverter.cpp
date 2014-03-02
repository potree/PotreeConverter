


#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "BinPointReader.h"

#include <liblas/liblas.hpp>

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>

using std::stringstream;
using std::map;
using std::string;
using std::vector;
using std::find;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

long addAncestorTime = 0;
long addSourceTime = 0;
long saveCloudJSTime = 0;

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

void PotreeConverter::initReader(){
	string fname = toUpper(fData);
	if(endsWith(fname, ".LAS") || endsWith(fname, ".LAZ")){
		cout << "creating LAS reader" << endl;
		reader = new LASPointReader(fname);
	}else if(endsWith(fname, ".BIN")){
		cout << "creating bin reader" << endl;
		reader = new BinPointReader(fname);
	}else{
		cout << "filename did not match a known format" << endl;
	}

	
}

void PotreeConverter::convert(){
	convert(reader->numPoints());
}


void PotreeConverter::convert(int numPoints){
	string dataDir = workDir + "/data";
	string tempDir = workDir + "/temp";
	system(("mkdir \"" + dataDir + "\"").c_str());
	system(("mkdir \"" + tempDir + "\"").c_str());

	//aabb = readAABB(fData, numPoints);

	//std::ifstream ifs;
	//ifs.open(fData, std::ios::in | std::ios::binary);
	//liblas::ReaderFactory f;
	//liblas::Reader reader = f.CreateWithStream(ifs);
	//liblas::Header const& header = reader.GetHeader();
	//liblas::Bounds<double> const &extent = header.GetExtent();
	//Vector3 min = Vector3(extent.minx(), extent.miny(), extent.minz());
	//Vector3 max = Vector3(extent.maxx(), extent.maxy(), extent.maxz());
	//aabb = AABB(min, max);
	aabb = reader->getAABB();

	cloudJs.clear();
	cloudJs << "{" << endl;
	cloudJs << "\t" << "\"version\": \"" << POTREE_FORMAT_VERSION << "\"," << endl;
	cloudJs << "\t" << "\"octreeDir\": \"data\"," << endl;
	cloudJs << "\t" << "\"boundingBox\": {" << endl;
	cloudJs << "\t\t" << "\"lx\": " << aabb.min.x << "," << endl;
	cloudJs << "\t\t" << "\"ly\": " << aabb.min.y << "," << endl;
	cloudJs << "\t\t" << "\"lz\": " << aabb.min.z << "," << endl;
	cloudJs << "\t\t" << "\"ux\": " << aabb.max.x << "," << endl;
	cloudJs << "\t\t" << "\"uy\": " << aabb.max.y << "," << endl;
	cloudJs << "\t\t" << "\"uz\": " << aabb.max.z << endl;
	cloudJs << "\t" << "}," << endl;
	cloudJs << "\t" << "\"pointAttributes\": [" << endl;
	cloudJs << "\t\t" << "\"POSITION_CARTESIAN\"," << endl;
	cloudJs << "\t\t" << "\"COLOR_PACKED\"" << endl;
	cloudJs << "\t" << "]," << endl;
	cloudJs << "\t" << "\"hierarchy\": [" << endl;

	{ // handle root
		cout << "processing root" << endl;
		SparseGrid grid(aabb, minGap);
		int pointsRead = 0;

		ofstream srOut(workDir + "/data/r", ios::out | ios::binary);
		ofstream sdOut(workDir + "/temp/d", ios::out | ios::binary);
		int numAccepted = 0;
		float minGapAtMaxDepth = minGap / pow(2.0f, maxDepth);
		int i = 0;
		while(reader->readNextPoint()){
			//liblas::Point const &point = reader.GetPoint();
			//float x = point.GetX();
			//float y = point.GetY();
			//float z = point.GetZ();
			//char r = (unsigned char)(float(point.GetColor().GetRed()) / 256.0f);
			//char g = (unsigned char)(float(point.GetColor().GetGreen()) / 256.0f);
			//char b = (unsigned char)(float(point.GetColor().GetBlue()) / 256.0f);
			//Point p(x,y,z, r, g, b);
			//if(i < 10){
			//	cout << p << endl;
			//}
			Point p = reader->getPoint();

			bool accepted = grid.add(p);
			int index = nodeIndex(aabb, p);
			if(accepted){
				// write point to ./data/r-file
				srOut.write((const char*)&p, sizeof(Point));
				numAccepted++;
			}else{
				// write point to ./temp/d-file
				sdOut.write((const char*)&p, sizeof(Point));
			}
			i++;
		}
		//delete[] buffer;
		cloudJs << "\t\t" << "[\"r\"," << numAccepted << "]," << endl;

		srOut.close();
		sdOut.close();
	}
	reader->close();

	//{ // handle root
	//	cout << "processing root" << endl;
	//	SparseGrid grid(aabb, minGap);

	//	ifstream sIn(fData, ios::in | ios::binary);
	//	int pointsRead = 0;
	//	int batchSize = min(10*1000*1000, numPoints);
	//	int batchByteSize = 4*batchSize*sizeof(float);
	//	//char *buffer = new char[batchByteSize];
	//	float *points = reinterpret_cast<float*>(buffer);
	//	char *cPoints = buffer;

	//	ofstream srOut(workDir + "/data/r", ios::out | ios::binary);
	//	ofstream sdOut(workDir + "/temp/d", ios::out | ios::binary);
	//	int numAccepted = 0;
	//	float minGapAtMaxDepth = minGap / pow(2.0f, maxDepth);
	//	while(pointsRead < numPoints){
	//		sIn.read(buffer, batchByteSize);
	//		long pointsReadRightNow = (long)(sIn.gcount() / (4*sizeof(float)));
	//		pointsRead += pointsReadRightNow;
	//		//cout << "pointsRead: " << pointsRead << endl;

	//		for(long i = 0; i < pointsReadRightNow; i++){
	//			float x = points[4*i+0];
	//			float y = points[4*i+1];
	//			float z = points[4*i+2];
	//			char r = cPoints[16*i+12];
	//			char g = cPoints[16*i+13];
	//			char b = cPoints[16*i+14];
	//			Point p(x,y,z, r, g, b);
	//			//float gap = MAX_FLOAT;
	//			bool accepted = grid.add(p);
	//			int index = nodeIndex(aabb, p);
	//			if(accepted){
	//				// write point to ./data/r-file
	//				srOut.write((const char*)&p, sizeof(Point));
	//				numAccepted++;
	//			}else{
	//				// write point to ./temp/d-file
	//				//if(gap > minGapAtMaxDepth){
	//					sdOut.write((const char*)&p, sizeof(Point));
	//				//}
	//			}
	//		}
	//	}
	//	//delete[] buffer;
	//	cloudJs << "\t\t" << "[\"r\"," << numAccepted << "]," << endl;

	//	srOut.close();
	//	sdOut.close();
	//}


	//string source = workDir + "/temp/d";
	string source = workDir + "/temp/d";
	string target = workDir + "/data/r";
	int depth = 1;
	//map<string, string> work;
	//work[source] = target;
	vector<Task> work;
	work.push_back(Task(source, target, aabb));

	// process points in breadth first order
	while(!work.empty() && depth <= maxDepth){
		cout << "processing level " << depth << endl;
		////map<string, string> nextRound;
		vector<Task> nextRound;

		//map<string, string>::iterator it;
		vector<Task>::iterator it;
		for(it = work.begin(); it != work.end(); it++){
			//source = it->first;
			//target = it->second;
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
				//nextRound[ssSource.str()] = ssTarget.str();
				AABB chAABB = childAABB(aabb, index);
				Task task(ssSource.str(), ssTarget.str(), chAABB);
				nextRound.push_back(task);
			}
		}

		depth++;
		work = nextRound;
	}


	stringstream ssCloudJs;
	string strCloudJs = cloudJs.str();
	ssCloudJs << strCloudJs.erase(strCloudJs.find_last_of(",")) << endl;
	ssCloudJs << "\t]" << endl;
	ssCloudJs << "}" << endl;

	ofstream sCloudJs(workDir + "/cloud.js", ios::out);
	sCloudJs << ssCloudJs.str();
	sCloudJs.close();

	delete[] buffer;

	float addAncestorDuration = float(addAncestorTime)/1000.0f;
	float addSourceDuration = float(addSourceTime)/1000.0f;
	float saveCloudJSDuration = float(saveCloudJSTime)/1000.0f;
	cout << "addAncestorDuration: " << addAncestorDuration << "s" << endl;
	cout << "addSourceDuration: " << addSourceDuration << "s" << endl;
	cout << "saveCloudJSDuration: " << saveCloudJSDuration << "s" << endl;

	/*cloudJs << "\t]" << endl;
	cloudJs << "}" << endl;

	ofstream sCloudJs(workDir + "/cloud.js", ios::out);
	sCloudJs << cloudJs.str();
	sCloudJs.close();*/
}

/**
 * 1. load all points from upper levels (r, rx, rxy, until r...depth) and add them to the grid 
 * 2. load points from source and add them to the grid
 * 3. accept all points from source which have a minimum distance of minGap / 2^depth to all 
 *    points inside the grid
 * 4. save accepted points of source to target + index and the remaining points to source + index
 *
 * @returns the indices of the nodes that have been created 
 *  if nodes [r11, r13, r17] were created, then indices [1,3,7] will be returned
 *
 **/
vector<int> PotreeConverter::process(string source, string target, AABB aabb, int depth){
	vector<int> indices;
	//cout << "== processing node ==" << endl;
	////cout << "process(" << source << ", " << target << ", " << depth << ");" << endl;
	//cout << "source: " << source << endl;
	//cout << "target: " << target << endl;
	//cout << "aabb: " << aabb << endl;
	//cout << "depth: " << depth << endl;
	//AABB aabb = readAABB(source);

	{
		stringstream ssName;
		ssName << source.substr(source.find_last_of("d"), source.size() - source.find_last_of("d"));
		stringstream ssAABB;
		ssAABB << workDir << "/aabb/" << ssName.str();
		ofstream fAABB(ssAABB.str(), ios::out);
		fAABB << "min: " << aabb.min << endl;
		fAABB << "max: " << aabb.max << endl;
		fAABB.close();
	}

	SparseGrid grid(aabb, minGap / pow(2.0, depth));

	//cout << "adding ancestors" << endl;
	{// add all ancestors to the grid
		auto start = high_resolution_clock::now();

		//cout << "adding ancestors to grid" << endl;
		int pos = target.find_last_of("r");
		int numAncestors = target.size() - pos;
		string ancestor = target;
		for(int i = 0; i < numAncestors; i++){
			//cout << "adding " << ancestor << endl;
			ifstream sIn(ancestor, ios::in | ios::binary);
			int pointsRead = 0;
			int batchSize = 10*1000*1000;
			int batchByteSize = 4*batchSize*sizeof(float);
			//char *buffer = new char[batchByteSize];
			float *points = reinterpret_cast<float*>(buffer);
			char *cPoints = buffer;
			bool done = false;
			while(!done){
				sIn.read(buffer, batchByteSize);
				int pointsReadRightNow = sIn.gcount() / (4*sizeof(float));
				done = pointsReadRightNow == 0;

				for(int i = 0; i < pointsReadRightNow; i++){
					float x = points[4*i+0];
					float y = points[4*i+1];
					float z = points[4*i+2];
					char r = cPoints[16*i+12];
					char g = cPoints[16*i+13];
					char b = cPoints[16*i+14];
					Point p(x,y,z, r, g, b);
					if(aabb.isInside(p)){
						grid.addWithoutCheck(p);
					}
				}
			}
			//delete[] buffer;

			ancestor = ancestor.substr(0, ancestor.size()-1);
		}
		auto end = high_resolution_clock::now();
		addAncestorTime += duration_cast<milliseconds>(end-start).count();
	}

	//cout << "adding source points" << endl;
	{ // add source to the grid
		auto start = high_resolution_clock::now();
		ifstream sIn(source, ios::in | ios::binary);
		int pointsRead = 0;
		int batchSize = 10*1000*1000;
		int batchByteSize = 4*batchSize*sizeof(float);
		//char *buffer = new char[batchByteSize];
		float *points = reinterpret_cast<float*>(buffer);
		char *cPoints = buffer;
		bool done = false;
		vector<ofstream> srOut;
		vector<ofstream> sdOut;
		vector<int> numPoints;
		numPoints.resize(8);
		for(int i = 0; i < 8; i++){
			stringstream ssr, ssd;
			ssr << target << i;
			ssd << source << i;
			srOut.push_back(ofstream(ssr.str(), ios::out | ios::binary));
			sdOut.push_back(ofstream(ssd.str(), ios::out | ios::binary));
		}
		long pointsProcessed = 0;
		while(!done){
			sIn.read(buffer, batchByteSize);
			int pointsReadRightNow = sIn.gcount() / (4*sizeof(float));
			done = pointsReadRightNow == 0;

			for(int i = 0; i < pointsReadRightNow; i++){
				if((pointsProcessed % 1000) == 0){
					//cout << "processing point " << pointsProcessed << endl;
				}
				
				float x = points[4*i+0];
				float y = points[4*i+1];
				float z = points[4*i+2];
				char r = cPoints[16*i+12];
				char g = cPoints[16*i+13];
				char b = cPoints[16*i+14];
				Point p(x,y,z, r, g, b);

				//float gap = MAX_FLOAT;
				bool accepted = grid.add(p);
				int index = nodeIndex(aabb, p);
				if(index == -1){
					continue;
				}
				//AABB chAABB = childAABB(aabb, index);
				//if(!chAABB.isInside(p)){
				//	cout << "ohje..." << endl;
				//}
				//if(index == 1){
				//	cout << "index is 1" << endl;
				//}
				if(accepted){
					srOut[index].write((const char*)&p, sizeof(Point));
					if(find(indices.begin(), indices.end(), index) == indices.end()){
						indices.push_back(index);
					}
					numPoints[index]++;
				}else{
					//if(gap > minGap / pow(2.0f, maxDepth)){
						sdOut[index].write((const char*)&p, sizeof(Point));
					//}
				}
				pointsProcessed++;
			}
		}

		for(int i = 0; i < 8; i++){
			srOut[i].close();
			sdOut[i].close();

			{// remove empty files
				stringstream ssr, ssd;
				ssr << target << i;
				ssd << source << i;
				if(filesize(ssr.str()) == 0){
					remove(ssr.str().c_str());
				}
				if(filesize(ssd.str()) == 0){
					remove(ssd.str().c_str());
				}
			}


			if(numPoints[i] > 0){
				stringstream ssName;
				ssName << target.substr(target.find_last_of("r"), target.size() - target.find_last_of("r")) << i;
				cout << "created node: " <<  ssName.str() << endl;
			}
		}

		auto end = high_resolution_clock::now();
		addSourceTime += duration_cast<milliseconds>(end-start).count();
		

		//delete[] buffer;

		{ // save current cloud.js
			auto start = high_resolution_clock::now();

			for(int i = 0; i < 8; i++){
				if(numPoints[i] > 0){
					stringstream ssName;
					ssName << target.substr(target.find_last_of("r"), target.size() - target.find_last_of("r")) << i;
					cloudJs << "\t\t" << "[\"" << ssName.str() << "\", " << numPoints[i] << "],";
					cloudJs << endl;
				}
			}

			stringstream ssCloudJs;
			string strCloudJs = cloudJs.str();
			ssCloudJs << strCloudJs.erase(strCloudJs.find_last_of(",")) << endl;
			ssCloudJs << "\t]" << endl;
			ssCloudJs << "}" << endl;

			ofstream sCloudJs(workDir + "/cloud.js", ios::out);
			sCloudJs << ssCloudJs.str();
			sCloudJs.close();

			auto end = high_resolution_clock::now();
			saveCloudJSTime += duration_cast<milliseconds>(end-start).count();
		}
	}
	cout << "== finished processing node ==" << endl << endl;

	return indices;
}