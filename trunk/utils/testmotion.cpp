
#include <fstream>
#include <iostream>

#include "obme.h"

using namespace std;
using namespace rududu;

#define WIDTH	1280
#define HEIGHT	720
#define CMPNT	3

#define ALIGN	32

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	COBME obme(WIDTH >> 3, HEIGHT >> 3);
	CImage inImage(WIDTH, HEIGHT, CMPNT,  ALIGN);
	CImage tmpImage(WIDTH, HEIGHT, CMPNT, ALIGN);
	CImage outImage(WIDTH, HEIGHT, CMPNT, ALIGN);
	unsigned char * tmp = new unsigned char[WIDTH * HEIGHT * CMPNT];
	CImage * inImages[2] = {&inImage, & tmpImage};
	int cur_image = 0;
	short * pIm[2];

	while(! cin.eof()) {
		cin.read((char*)tmp, WIDTH * HEIGHT * CMPNT);
		inImages[cur_image]->inputSGI(tmp, WIDTH, -128);
		pIm[0] = inImages[cur_image]->get_pImage(0);
		pIm[1] = inImages[1-cur_image]->get_pImage(0);
		obme.EPZS(WIDTH, HEIGHT, inImage.get_dimXAlign(), pIm);
		obme.apply_mv(inImages[1-cur_image], outImage);
		outImage -= *inImages[cur_image];
		outImage.outputYV12<char, false>((char*)tmp, WIDTH, -128);
		cout.write((char*)tmp, WIDTH * HEIGHT * CMPNT / 2);
		cur_image = 1-cur_image;
	}

	cout.flush();

	return 0;
}
