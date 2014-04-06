
#include <vector>
#include <map>
#include <iostream>
#include <math.h>
#include <string>
#include <fstream>

//#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "Vector3.h"
#include "AABB.h"
#include "Point.h"
#include "GridIndex.h"
#include "SparseGrid.h"
#include "GridCell.h"

using std::ifstream;
using std::ofstream;
using std::ios;
using std::string;
using std::min;
using std::max;
using std::ostream;
using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::binary_function;
using std::map;

AABB readAABB(string fIn, int numPoints){
	cout << "reading AABB from file" << endl;
	ifstream stream(fIn, std::ios::in | std::ios::binary);

	int pointsRead = 0;

	float maxFloat = std::numeric_limits<float>::max();
	float minFloat = -maxFloat;
	float minp[] = { maxFloat, maxFloat, maxFloat };
	float maxp[] = { minFloat, minFloat, minFloat };

	int batchsize = min(10*1000*1000, numPoints);
	int batchByteSize = 4*batchsize*sizeof(float);
	char *buffer = new char[batchByteSize];
	float *fBuffer = reinterpret_cast<float*>(buffer);
	while(pointsRead < numPoints){
		stream.read(buffer, batchByteSize);
		long pointsReadRightNow = (long)(stream.gcount() / (4*sizeof(float)));
		pointsRead += pointsReadRightNow;
		cout << "pointsRead: " << pointsRead << endl;

		for(long i = 0; i < pointsReadRightNow; i++){
			float x = fBuffer[4*i+0];
			float y = fBuffer[4*i+1];
			float z = fBuffer[4*i+2];

			minp[0] = min(x, minp[0]);
			minp[1] = min(y, minp[1]);
			minp[2] = min(z, minp[2]);

			maxp[0] = max(x, maxp[0]);
			maxp[1] = max(y, maxp[1]);
			maxp[2] = max(z, maxp[2]);
		}
	}
	delete buffer;
	stream.close();
	AABB aabb(Vector3(minp[0], minp[1], minp[2]), Vector3(maxp[0], maxp[1], maxp[2]));

	cout << "pointsRead: " << pointsRead << endl;
	cout << "min: " << minp[0] << ", " << minp[1] << ", " << minp[2] << endl;
	cout << "max: " << maxp[0] << ", " << maxp[1] << ", " << maxp[2] << endl;

	return aabb;
}

AABB readAABB(string fIn){
	//cout << "reading AABB from file" << endl;
	ifstream stream(fIn, std::ios::in | std::ios::binary);

	int pointsRead = 0;

	float maxFloat = std::numeric_limits<float>::max();
	float minFloat = -maxFloat;
	float minp[] = { maxFloat, maxFloat, maxFloat };
	float maxp[] = { minFloat, minFloat, minFloat };

	int batchsize = 10*1000*1000;
	int batchByteSize = 4*batchsize*sizeof(float);
	char *buffer = new char[batchByteSize];
	float *fBuffer = reinterpret_cast<float*>(buffer);
	bool done = false;
	while(!done){
		stream.read(buffer, batchByteSize);
		long pointsReadRightNow = (long)(stream.gcount() / (4*sizeof(float)));
		pointsRead += pointsReadRightNow;
		//cout << "pointsRead: " << pointsRead << endl;

		for(long i = 0; i < pointsReadRightNow; i++){
			float x = fBuffer[4*i+0];
			float y = fBuffer[4*i+1];
			float z = fBuffer[4*i+2];

			minp[0] = min(x, minp[0]);
			minp[1] = min(y, minp[1]);
			minp[2] = min(z, minp[2]);

			maxp[0] = max(x, maxp[0]);
			maxp[1] = max(y, maxp[1]);
			maxp[2] = max(z, maxp[2]);
		}

		done = pointsReadRightNow == 0;
	}
	delete buffer;
	stream.close();
	AABB aabb(Vector3(minp[0], minp[1], minp[2]), Vector3(maxp[0], maxp[1], maxp[2]));

	//cout << "pointsRead: " << pointsRead << endl;
	//cout << "min: " << minp[0] << ", " << minp[1] << ", " << minp[2] << endl;
	//cout << "max: " << maxp[0] << ", " << maxp[1] << ", " << maxp[2] << endl;

	return aabb;
}

/**
 *   y 
 *   |-z
 *   |/
 *   O----x
 *    
 *   3----7
 *  /|   /|
 * 2----6 |
 * | 1--|-5
 * |/   |/
 * 0----4
 *
 */
AABB childAABB(const AABB &aabb, const int &index){
	Vector3 min, max;
	Vector3 halfSize = aabb.size / 2.0f;

	min = aabb.min;
	max = aabb.min + halfSize;

	if(index == 0){
		
	}else if(index == 1){
		min = min + Vector3(0,0,halfSize.z);
		max = max + Vector3(0,0,halfSize.z);
	}else if(index == 2){
		min = min + Vector3(0, halfSize.y, 0);
		max.y = aabb.max.y;
	}else if(index == 3){
		min = min + Vector3(0, halfSize.y, halfSize.z);
		max.y = aabb.max.y;
		max.z = aabb.max.z;
	}else if(index == 4){
		min = min + Vector3(halfSize.x, 0, 0);
		max = max + Vector3(halfSize.x, 0, 0);
	}else if(index == 5){
		min = min + Vector3(halfSize.x, 0, halfSize.z);
		//max = max + Vector3(halfSize.x, 0, halfSize.z);
		max.x = aabb.max.x;
		max.z = aabb.max.z;
	}else if(index == 6){
		min = min + Vector3(halfSize.x, halfSize.y, 0);
		//max = max + Vector3(halfSize.x, halfSize.y, 0);
		max.x = aabb.max.x;
		max.y = aabb.max.y;
	}else if(index == 7){
		min = min + halfSize;
		//max = max + halfSize;
		max = aabb.max;
	}

	return AABB(min, max);
}


/**
 *   y 
 *   |-z
 *   |/
 *   O----x
 *    
 *   3----7
 *  /|   /|
 * 2----6 |
 * | 1--|-5
 * |/   |/
 * 0----4
 *
 */
int nodeIndex(const AABB &aabb, const Point &point){

	for(int i = 0; i < 8; i++){
		if(childAABB(aabb, i).isInside(point)){
			return i;
		}
	}
	
	return -1;
	
	//float x = (point.x - aabb.min.x) / aabb.size.x;
	//float y = (point.y - aabb.min.y) / aabb.size.y;
	//float z = (point.z - aabb.min.z) / aabb.size.z;

	//if(x < 0.5 ){
	//	if(y < 0.5){
	//		if(z < 0.5){
	//			return 0;
	//		}else{
	//			return 1;
	//		}
	//	}else{
	//		if(z < 0.5){
	//			return 2;
	//		}else{
	//			return 3;
	//		}
	//	}
	//}else{
	//	if(y < 0.5){
	//		if(z < 0.5){
	//			return 4;
	//		}else{
	//			return 5;
	//		}
	//	}else{
	//		if(z < 0.5){
	//			return 6;
	//		}else{
	//			return 7;
	//		}
	//	}
	//}
}




/**
 * from http://stackoverflow.com/questions/5840148/how-can-i-get-a-files-size-in-c
 */
long filesize(string filename){
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}


/**
 * from http://stackoverflow.com/questions/874134/find-if-string-endswith-another-string-in-c
 */
bool endsWith (std::string const &fullString, std::string const &ending)
{
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

/**
 * see http://stackoverflow.com/questions/735204/convert-a-string-in-c-to-upper-case
 */
string toUpper(string str){
	string tmp = str;
	std::transform(tmp.begin(), tmp.end(),tmp.begin(), ::toupper);

	return tmp;
}
