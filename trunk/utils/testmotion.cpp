
#include <fstream>
#include <iostream>

#include "obme.h"
#include "huffcodec.h"

using namespace std;
using namespace rududu;

#define WIDTH	1280
#define HEIGHT	720
#define CMPNT	3

#define ALIGN	32

sHuffSym hufftable[] = {
	{12, 0, 0},
	{8, 0, 1},
	{125, 0, 2},
	{741, 0, 3},
	{65, 0, 4},
	{9, 0, 5},
	{2, 0, 6},
	{78, 0, 7},
	{93, 0, 8},
	{52, 0, 9},
	{512, 0, 10},
	{3, 0, 11},
};

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	COBME obme(WIDTH >> 3, HEIGHT >> 3);
	COBMC obmc(WIDTH >> 3, HEIGHT >> 3);
	CImage inImage(WIDTH, HEIGHT, CMPNT,  ALIGN);
	CImage tmpImage(WIDTH, HEIGHT, CMPNT, ALIGN);
	CImage outImage(WIDTH, HEIGHT, CMPNT, ALIGN);
	unsigned char * tmp = new unsigned char[WIDTH * HEIGHT * CMPNT];
	CImage * inImages[2] = {&inImage, & tmpImage};
	int cur_image = 0;
	short * pIm[2];

	unsigned char * pStream = new unsigned char[WIDTH * HEIGHT];
	CMuxCodec Codec(pStream, 0);
	unsigned char * pEnd;
	unsigned int total_mv_size = 0;

	while(! cin.eof()) {
		cin.read((char*)tmp, WIDTH * HEIGHT * CMPNT);
		inImages[cur_image]->inputSGI(tmp, WIDTH, -128);
		pIm[0] = inImages[cur_image]->get_pImage(0);
		pIm[1] = inImages[1-cur_image]->get_pImage(0);
		obme.EPZS(WIDTH, HEIGHT, inImage.get_dimXAlign(), pIm);

		Codec.initCoder(0, pStream);
		obme.encode(& Codec);
		pEnd = Codec.endCoding();
 		cerr << "mv size : " << (int)(pEnd - pStream) << endl;
		total_mv_size += (unsigned int)(pEnd - pStream);

// 		Codec.initDecoder(pStream);
// 		obmc.decode(& Codec);

// 		obmc.apply_mv(inImages[1-cur_image], outImage);
// 		outImage -= *inImages[cur_image];
// 		outImage.outputYV12<char, false>((char*)tmp, WIDTH, -128);
// 		cout.write((char*)tmp, WIDTH * HEIGHT * CMPNT / 2);
		cur_image = 1-cur_image;
	}

// 	CHuffCodec::make_huffman(hufftable, sizeof(hufftable) / sizeof(sHuffSym));

	cerr << "total : " << total_mv_size << endl;
	cout.flush();

	return 0;
}
