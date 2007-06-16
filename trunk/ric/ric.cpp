
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <math.h>
#include <unistd.h>

#include <Magick++.h>

#include <utils.h>
#include <wavelet2d.h>
#include <dct2d.h>

using namespace std;
using namespace Magick;
using namespace rududu;

#define BAD_MAGIC		2
#define UNKNOW_TYPE		3
#define SHIFT			4

short Quants(int idx)
{
	static const unsigned short Q[5] = {32768, 37641, 43238, 49667, 57052};
	if (idx == 0) return 0; // lossless
	idx--;
	int r = 14 - SHIFT - idx / 5;
	return (short)((Q[idx % 5] + (1 << (r - 1))) >> r );
}

template <class Pxl>
void BW2RGB(Pxl * pIn, int stride, Pxl offset = 0, Pxl scale = 1)
{
	for( int i = stride - 1, j = (stride - 1) * 3; i > 0 ; i--){
		pIn[j] = pIn[j+1] = pIn[j+2] = pIn[i] * scale + offset;
		j-=3;
	}
}

template <unsigned int shift>
void short2char(short * pIn, int size)
{
	unsigned char * pOut = (unsigned char *) pIn;
	for( int i = 0; i < size ; i++){
		if (shift > 0)
			pIn[i] = 128 + ((pIn[i] + (1 << (shift - 1))) >> shift);
		else
			pIn[i] += 128;
		pOut[i] = (unsigned char) CLIP(pIn[i], 0, 255);
	}
}

template <unsigned int shift>
void char2short(short * pOut, int size)
{
	unsigned char * pIn = (unsigned char *) pOut;
	for( int i = size - 1; i >= 0 ; i--){
		pOut[i] = (pIn[i] - 128) << shift;
	}
}

#define WAV_LEVELS	5
#define TRANSFORM	cdf53

typedef union {
	struct  {
		unsigned char Quant	:5;
		unsigned char Type	:3;
	};
	char last;
} Header;

void CompressImage(string & infile, string & outfile, int Quant, float Thres)
{
	Header Head;
	Head.Quant = Quant;
	Head.Type = 0;

	ofstream oFile( outfile.c_str() , ios::out );
	oFile << "RUD2";

	Image img( infile );
	img.type( GrayscaleType );
	unsigned int imSize = img.columns() * img.rows();
	short * ImgPixels = new short [imSize];
	unsigned char * pStream = new unsigned char[imSize];

	img.write(0, 0, img.columns(), img.rows(), "R", CharPixel, ImgPixels);
	if (Quant == 0)
		char2short<0>(ImgPixels, imSize);
	else
		char2short<SHIFT>(ImgPixels, imSize);

	unsigned short tmp = img.columns();
	oFile.write((char *)&tmp, sizeof(unsigned short));
	tmp = img.rows();
	oFile.write((char *)&tmp, sizeof(unsigned short));
	oFile.write((char *)&Head, sizeof(Header));
	unsigned char * pEnd = pStream;

	CMuxCodec Codec(pEnd, 0);

	CWavelet2D Wavelet(img.columns(), img.rows(), WAV_LEVELS);
	Wavelet.SetWeight(TRANSFORM);
	Wavelet.Transform<TRANSFORM>(ImgPixels, img.columns());
	Wavelet.TSUQ(Quants(Quant), Thres);
	Wavelet.CodeBand(&Codec, 1);

	pEnd = Codec.endCoding();

	oFile.write((char *) pStream + 2, (pEnd - pStream - 2));
	oFile.close();

	delete[] ImgPixels;
	delete[] pStream;
}

void DecompressImage(string & infile, string & outfile, int Dither)
{
	ifstream iFile( infile.c_str() , ios::in );
	char magic[4] = {0,0,0,0};

	iFile.read(magic, 4);

	if (magic[0] != 'R' || magic[1] != 'U' || magic[2]!= 'D' || magic[3] != '2')
		throw BAD_MAGIC;

	unsigned short width, heigth;
	iFile.read((char *) &width, sizeof(unsigned short));
	iFile.read((char *) &heigth, sizeof(unsigned short));

	short * ImgPixels = new short [width * heigth * 3];
	unsigned char * pStream = new unsigned char[width * heigth];

	iFile.read((char *) pStream, width * heigth);

	Header Head;
	Head.last = *pStream;

	unsigned char * pEnd = pStream;

	CMuxCodec Codec(pEnd - 1);

	CWavelet2D Wavelet(width, heigth, WAV_LEVELS);
	Wavelet.SetWeight(TRANSFORM);
	Wavelet.DecodeBand(&Codec, 1);
	Wavelet.TSUQi(Quants(Head.Quant));
	Wavelet.TransformI<TRANSFORM>(ImgPixels, width);

	if (Head.Quant == 0)
		short2char<0>(ImgPixels, width * heigth);
	else
		short2char<SHIFT>(ImgPixels, width * heigth);

	BW2RGB((unsigned char *)ImgPixels, width * heigth);
	Image img(width, heigth, "RGB", CharPixel, ImgPixels);
	img.type( GrayscaleType );
	img.depth(8);
	img.compressType(UndefinedCompression);
	// img.display();

	img.write(outfile);

	delete[] ImgPixels;
	delete[] pStream;
}

void Test(string & infile, int Quant)
{
	Image img( infile );
	img.type( GrayscaleType );
	unsigned int imSize = img.columns() * img.rows();
	short * ImgPixels = new short [imSize * 3];

	img.write(0, 0, img.columns(), img.rows(), "R", ShortPixel, ImgPixels);
	for( int i = 0; i < imSize; i++)
		ImgPixels[i] = ((short)(ImgPixels[i] + (1 << 15))) >> 6;

	CDCT2D dct(img.columns(), img.rows());
// 	dct.Proc<true>(ImgPixels, img.columns());
	dct.Transform<true>(ImgPixels, img.columns());
 	dct.TSUQ(Quants(Quant), .7f);
 	dct.TSUQi(Quants(Quant));
	dct.Transform<false>(ImgPixels, img.columns());
// 	dct.Proc<false>(ImgPixels, img.columns());

	for( int i = 0; i < imSize; i++) {
		int tmp = (ImgPixels[i] << 6) + (1 << 15);
		if (tmp > ((1 << 16) - 1)) tmp = ((1 << 16) - 1);
		if (tmp < 0) tmp = 0;
		ImgPixels[i] = (short) tmp;
	}

	BW2RGB(ImgPixels, imSize);
	Image img2(img.columns(), img.rows(), "RGB", ShortPixel, ImgPixels);
	img2.display();

	delete[] ImgPixels;
}

int main( int argc, char *argv[] )
{
	int c;
	extern char * optarg;
	extern int optind, opterr;
	char * progname = argv[0];
	string infile;
	string outfile;
	float ThresRatio = 0.7;
	int Quant = 9;
	float RecLevelRatio = 0;
	int Type = 0;
	int Dither = 0;

	while ((c = getopt(argc , argv, "i:o:q:r:t:v:d")) != -1) {
		switch (c) {
			case 'i':
				infile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'q':
				Quant = atoi(optarg);
				break;
			case 'r':
				RecLevelRatio = atof(optarg);
				break;
			case 't':
				ThresRatio = atof(optarg);
				break;
			case 'v':
				Type = atoi(optarg);
				break;
			case 'd':
				Dither = 1;
		}
	}
	if (infile.length() == 0) {
		cout << "An input file name must be specified (option -i)" << endl;
		exit(1);
	}

	int mode = 0; // 0 = code , 1 = decode
	int loc;
	if ((loc = infile.rfind(".ric", string::npos, 4)) != string::npos){
		// mode décodage
		mode = 1;
		if (outfile.length() == 0) {
			outfile = infile;
// 			outfile.resize(loc);
			outfile.append(".pgm");
		}
	} else {
		// mode codage
		mode = 0;
		if (outfile.length() == 0) {
			outfile = infile;
			loc = infile.find_last_of(".", string::npos, 5);
			int loc2 = infile.find_last_of("/");
			if (loc != string::npos && loc2 < loc) {
				outfile.resize(loc);
			}
			outfile.append(".ric");
		}
	}


	if (mode == 0) {
// 		CompressImage(infile, outfile, Quant, ThresRatio);
		Test(infile, Quant);
	} else {
		DecompressImage(infile, outfile, Dither);
	}

	return EXIT_SUCCESS;
}
