
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

	while(! cin.eof()) {
		inImage.inputRGB(cin, WIDTH, 0);
// 		obmc.apply_mv(& inImage, outImage);
		inImage.outputBW(cout, WIDTH, 0);
	}

	return 0;
}
