
#include <vector>
#include <map>
#include <iostream>
#include <math.h>
#include <string>
#include <fstream>

//#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <PlyPointReader.h>
#include <LASPointReader.h>

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
	Vector3<double> min, max;
	Vector3<double> halfSize = aabb.size / 2.0f;

	min = aabb.min;
	max = aabb.min + halfSize;

	if(index == 0){
		
	}else if(index == 1){
		min = min + Vector3<double>(0,0,halfSize.z);
		max = max + Vector3<double>(0,0,halfSize.z);
	}else if(index == 2){
		min = min + Vector3<double>(0, halfSize.y, 0);
		max.y = aabb.max.y;
	}else if(index == 3){
		min = min + Vector3<double>(0, halfSize.y, halfSize.z);
		max.y = aabb.max.y;
		max.z = aabb.max.z;
	}else if(index == 4){
		min = min + Vector3<double>(halfSize.x, 0, 0);
		max = max + Vector3<double>(halfSize.x, 0, 0);
	}else if(index == 5){
		min = min + Vector3<double>(halfSize.x, 0, halfSize.z);
		//max = max + Vector3<double>(halfSize.x, 0, halfSize.z);
		max.x = aabb.max.x;
		max.z = aabb.max.z;
	}else if(index == 6){
		min = min + Vector3<double>(halfSize.x, halfSize.y, 0);
		//max = max + Vector3<double>(halfSize.x, halfSize.y, 0);
		max.x = aabb.max.x;
		max.y = aabb.max.y;
	}else if(index == 7){
		min = min + halfSize;
		//max = max + halfSize;
		max = aabb.max;
	}

	return AABB(min, max);
}

//AABB childAABB(const AABB &aabb, const int &index){
//	Vector3<double> min, max;
//	Vector3<double> halfSize = aabb.size / 2.0f;
//
//	min = aabb.min;
//	max = aabb.min + halfSize;
//
//	// FIXME - for quadtree
//	if(index == 0){
//		
//	}else if(index == 1){
//		min = min + Vector3<double>(0,0,halfSize.z);
//		max = max + Vector3<double>(0,0,halfSize.z);
//	}else if(index == 2){
//		min = min + Vector3<double>(0, halfSize.y, 0);
//		max.y = aabb.max.y;
//	}else if(index == 3){
//		min = min + Vector3<double>(0, halfSize.y, halfSize.z);
//		max.y = aabb.max.y;
//		max.z = aabb.max.z;
//	}else if(index == 4){
//		min = min + Vector3<double>(halfSize.x, 0, 0);
//		max = max + Vector3<double>(halfSize.x, 0, 0);
//	}else if(index == 5){
//		min = min + Vector3<double>(halfSize.x, 0, halfSize.z);
//		//max = max + Vector3<double>(halfSize.x, 0, halfSize.z);
//		max.x = aabb.max.x;
//		max.z = aabb.max.z;
//	}else if(index == 6){
//		min = min + Vector3<double>(halfSize.x, halfSize.y, 0);
//		//max = max + Vector3<double>(halfSize.x, halfSize.y, 0);
//		max.x = aabb.max.x;
//		max.y = aabb.max.y;
//	}else if(index == 7){
//		min = min + halfSize;
//		//max = max + halfSize;
//		max = aabb.max;
//	}
//	min.z = aabb.min.z;
//	max.z = aabb.max.z;
//
//	return AABB(min, max);
//}


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
}


//int nodeIndex(const AABB &aabb, const Point &point){
//
//	for(int i = 0; i < 8; i++){
//		if(childAABB(aabb, i).isInside(point)){
//			//return i;
//
//			// FIXME - for quadtree
//			return i - (i%2);
//		}
//	}
//	
//	return -1;
//}



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

PointReader *createPointReader(string path){
	PointReader *reader = NULL;
	if(boost::iends_with(path, ".las") || boost::iends_with(path, ".laz")){
		reader = new LASPointReader(path);
	}else if(boost::iends_with(path, ".ply")){
		reader = new PlyPointReader(path);
	}

	return reader;
}

AABB calculateAABB(vector<string> sources){
	AABB aabb;

	for(int i = 0; i < sources.size(); i++){
		string source = sources[i];

		PointReader *reader = createPointReader(source);
		AABB lAABB = reader->getAABB();


		aabb.update(lAABB.min);
		aabb.update(lAABB.max);

		reader->close();
		delete reader;
	}

	return aabb;
}