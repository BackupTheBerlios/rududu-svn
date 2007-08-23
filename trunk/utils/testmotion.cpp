
#include <fstream>
#include <iostream>

#include "obmc.h"

using namespace std;
using namespace rududu;

#define WIDTH	1280
#define HEIGHT	720
#define CMPNT	3

#define ALIGN	32

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	rududu::COBMC obmc(WIDTH >> 3, HEIGHT >> 3);
	CImage inImage(WIDTH, HEIGHT, CMPNT,  ALIGN);
	CImage outImage(WIDTH, HEIGHT, CMPNT, ALIGN);
	unsigned char * tmp = new unsigned char[WIDTH * HEIGHT * CMPNT];

	while(! cin.eof()) {
		cin.read((char*)tmp, WIDTH * HEIGHT * CMPNT);
		inImage.inputSGI(tmp, WIDTH, -128);
		obmc.apply_mv(& inImage, outImage);
// 		outImage -= inImage;
		outImage.outputYV12<char, false>((char*)tmp, WIDTH, -128);
		cout.write((char*)tmp, WIDTH * HEIGHT * CMPNT / 2);
	}

	cout.flush();

	return 0;
}
