

#include <vector>

using std::vector;


template<T>
class Structure{
	
	vector<int> arr;

	T* get(int&x, int &y, int &z){
		int d = 64;
		int p = 2;
		for(int i = 0; i < 7; i++){
			int ix = x / d;
			int iy = y / d;
			int iz = z / d;

			int index = ix + iy * p + iz * p * p;

			int val = arr[index];
			if(val == 0){
				return NULL;
			}

			p *= 2;
			d /= 2;
		}
	}

};




int main(int argc, char **argv){

}


