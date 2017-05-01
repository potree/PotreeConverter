#include "stuff.h"

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



#if BOOST_OS_WINDOWS
#include <Windows.h>
#elif BOOST_OS_LINUX
#include <linux/limits.h>
#elif BOOST_OS_MACOS

#elif BOOST_OS_BSD

#endif



namespace Potree{

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

	Vector3<double> min = aabb.min;
	Vector3<double> max = aabb.max;

	if((index & 0b0001) > 0){
		min.z += aabb.size.z / 2;
	}else{
		max.z -= aabb.size.z / 2;
	}

	if((index & 0b0010) > 0){
		min.y += aabb.size.y / 2;
	}else{
		max.y -= aabb.size.y / 2;
	}

	if((index & 0b0100) > 0){
		min.x += aabb.size.x / 2;
	}else{
		max.x -= aabb.size.x / 2;
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
	int mx = (int)(2.0 * (point.position.x - aabb.min.x) / aabb.size.x);
	int my = (int)(2.0 * (point.position.y - aabb.min.y) / aabb.size.y);
	int mz = (int)(2.0 * (point.position.z - aabb.min.z) / aabb.size.z);

	mx = min(mx, 1);
	my = min(my, 1);
	mz = min(mz, 1);

	return (mx << 2) | (my << 1) | mz;
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

// http://stackoverflow.com/questions/8593608/how-can-i-copy-a-directory-using-boost-filesystem
bool copyDir(fs::path source, fs::path destination){
	
    try{
        // Check whether the function call is valid
        if(!fs::exists(source) || !fs::is_directory(source) ) {
            std::cerr << "Source directory " << source.string() << " does not exist or is not a directory." << '\n';
            return false;
        }
        //if(fs::exists(destination)){
        //    std::cerr << "Destination directory " << destination.string()
        //        << " already exists." << '\n';
        //    return false;
        //}
        // Create the destination directory
		if(!fs::exists(destination)){
			if(!fs::create_directory(destination)){
				std::cerr << "Unable to create destination directory" << destination.string() << '\n';
				return false;
			}
		}
    }catch(fs::filesystem_error const & e){
        std::cerr << e.what() << '\n';
        return false;
    }
    // Iterate through the source directory
    for( fs::directory_iterator file(source); file != fs::directory_iterator(); ++file){
        try{
            fs::path current(file->path());
            if(fs::is_directory(current)) {
                // Found directory: Recursion
                if(!copyDir(current, destination / current.filename())){
                    return false;
                }
            }else{
                // Found file: Copy
                fs::copy_file(current,destination / current.filename(), fs::copy_option::overwrite_if_exists);
            }
        }catch(fs::filesystem_error const & e){
            std:: cerr << e.what() << '\n';
        }
    }
    return true;
}


float psign(float value){
	if(value == 0.0){
		return 0.0;
	}else if(value < 0.0){
		return -1.0;
	}else{
		return 1.0;
	}
}



// see http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
string getExecutablePath(){

	string path = "./";
	
#if BOOST_OS_WINDOWS
	char  buffer[MAX_PATH]; 
	GetModuleFileName( NULL, buffer, MAX_PATH );

	string::size_type pos = string( buffer ).find_last_of( "\\/" );
	path = string( buffer ).substr( 0, pos);
#elif BOOST_OS_LINUX
	// http://stackoverflow.com/questions/5525668/how-to-implement-readlink-to-find-the-path
	char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
    if (len != -1) {
      buff[len] = '\0';
      path = string(buff);
    }else{
		cout << "WARNING: Potree was unable to determine to path to the executable." << endl;
		cout << "Using current work dir as executable directory. Make sure to run potree inside the directory with the executable to avoid problems." << endl;
	}
//#elif BOOST_OS_MACOS
	// TODO 
	// http://stackoverflow.com/questions/799679/programatically-retrieving-the-absolute-path-of-an-os-x-command-line-app
//#elif BOOST_OS_BSD
	// TODO
#else
	cout << "WARNING: Potree was unable to identify the operating system and as a result, the directory of the executable." << endl;
	cout << "Using current work dir as executable directory. Make sure to run potree inside the directory with the executable to avoid problems." << endl;
	path = "./";
#endif

	return path;
}

}
