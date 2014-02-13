
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <math.h>

using std::string;
using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using std::ofstream;
using std::max;
using std::min;


std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

void convertXYZ(string fIn, string fOut, int rgbMaxValue){
	ifstream stream(fIn);
	ofstream sOut(fOut, std::ios::binary);

	int i = 0;
	string line;
	while(stream.good()){
		getline(stream, line);
		vector<string> tokens = split(line, ' ');
		if(tokens.size() != 6 && tokens.size() != 7){
			continue;
		}
		float x = stof(tokens[0]);
		float y = stof(tokens[1]);
		float z = stof(tokens[2]);
		unsigned char r = 255 * stoi(tokens[3]) / rgbMaxValue;
		unsigned char g = 255 * stoi(tokens[4]) / rgbMaxValue;
		unsigned char b = 255 * stoi(tokens[5]) / rgbMaxValue;
		unsigned char a = 255;

		sOut.write((char*)&x, sizeof(float));
		sOut.write((char*)&y, sizeof(float));
		sOut.write((char*)&z, sizeof(float));
		sOut.write((char*)&r, sizeof(char));
		sOut.write((char*)&g, sizeof(char));
		sOut.write((char*)&b, sizeof(char));
		sOut.write((char*)&a, sizeof(char));

		if((i % 1000000) == 0){
			cout << i << "m points" << endl;
		}
		i++;
	}
	cout << i << "m points" << endl;

	stream.close();
	sOut.close();
}

int main(int argc, char **argv){
	if(argc != 4){
		cout << "usage: " << endl;
		cout << "xyzrgb2bin <sourcePath> <targetPath> <rgbMaxValue>" << endl;
		return 1;
	}

	string fIn = argv[1];
	string fOut = argv[2];
	int rgbMaxValue = atoi(argv[3]);

	convertXYZ(fIn, fOut, rgbMaxValue);
}

//xyzrgb2bin C:/dev/pointclouds/map.archi.fr/Pompei_Ortho.xyz C:/dev/pointclouds/map.archi.fr/Pompei_test.bin 255
//PotreeConverter.exe C:/dev/pointclouds/map.archi.fr/Pompei_test.bin C:/dev/pointclouds/map.archi.fr/pompei_test 1.0 5