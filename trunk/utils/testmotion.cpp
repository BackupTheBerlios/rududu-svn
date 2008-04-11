
#include <fstream>
#include <iostream>

#include "rududucodec.h"

using namespace std;
using namespace rududu;

#define WIDTH	1280
#define HEIGHT	720
#define CMPNT	3
// #define ALIGN	32

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	unsigned char * tmp = new unsigned char[WIDTH * HEIGHT * CMPNT];
 	unsigned char * pStream = new unsigned char[WIDTH * HEIGHT * CMPNT];
	CImage origin(WIDTH, HEIGHT, CMPNT, ALIGN);
	CRududuCodec encoder(rududu::encode, WIDTH, HEIGHT, CMPNT);
	CRududuCodec decoder(rududu::decode, WIDTH, HEIGHT, CMPNT);

	encoder.quant = 20;
	decoder.quant = 20;

	while(! cin.eof()) {
		cin.read((char*)tmp, WIDTH * HEIGHT * CMPNT);
		origin.inputSGI(tmp, WIDTH, -128);
		CImage * encOutImage = 0;
		int size_enc = encoder.encode(tmp, WIDTH, pStream, &encOutImage);
		CImage * outImage = 0;
		int size_dec = decoder.decode(pStream, &outImage);

		cerr << size_enc << "	" << size_dec << "	";
		float psnr[CMPNT];
		origin.psnr(*encOutImage, psnr);
		for( int c = 0; c < CMPNT; c++){
			cerr << psnr[c] << "	";
		}
		origin.psnr(*outImage, psnr);
		for( int c = 0; c < CMPNT; c++){
			cerr << psnr[c] << "	";
		}
		cerr << endl;
// 		origin -= *encOutImage;

		encOutImage->outputYV12<char, false>((char*)tmp, WIDTH, -128);
#ifndef DEBUG
 		cout.write((char*)tmp, WIDTH * HEIGHT * CMPNT / 2);
#endif
	}

	cout.flush();

	delete[] tmp;
	delete[] pStream;

	return 0;
}
