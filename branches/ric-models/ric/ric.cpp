/***************************************************************************
 *   Copyright (C) 2007-2008 by Nicolas Botti                              *
 *   <rududu@laposte.net>                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <fstream>
#include <iostream>

// http://cimg.sourceforge.net
#define cimg_display_type 0
#include <CImg.h>

#include <utils.h>
#include <wavelet2d.h>

using namespace std;
using namespace cimg_library;
using namespace rududu;

#define BAD_MAGIC		2
#define UNKNOW_TYPE		3
#define SHIFT			4

short Quants(int idx)
{
	static const unsigned short Q[5] = {32768, 37641, 43238, 49667, 57052};
	if (idx <= 0) return 0; // lossless
	idx--;
	int r = 14 - idx / 5;
	return (short)((Q[idx % 5] + (1 << (r - 1))) >> r );
}

void dither(short * pIn, int width, int heigth)
{
	for( int j = 0; j < heigth - 1; j++){
		pIn[0] = 128 + ((pIn[0] + (1 << (SHIFT - 1))) >> SHIFT);
		pIn[0] = CLIP(pIn[0], 0, 255);
		for( int i = 1; i < width - 1; i++){
			short tmp = pIn[i] + (1 << (SHIFT - 1));
			pIn[i] = tmp >> SHIFT;
			tmp -= pIn[i] << SHIFT;
			pIn[i+1] += (tmp >> 1) - (tmp >> 4);
			pIn[i+width - 1] += (tmp >> 3) + (tmp >> 4);
			pIn[i+width] += (tmp >> 2) + (tmp >> 4);
			pIn[i+width + 1] += tmp >> 4;
			pIn[i] = clip(pIn[i] + 128, 0, 255);
		}
		pIn += width;
		pIn[-1] = 128 + ((pIn[-1] + (1 << (SHIFT - 1))) >> SHIFT);
		pIn[-1] = CLIP(pIn[-1], 0, 255);
	}
	for( int i = 0; i < width; i++){
		pIn[i] = 128 + ((pIn[i] + (1 << (SHIFT - 1))) >> SHIFT);
		pIn[i] = CLIP(pIn[i], 0, 255);
	}
}

#define WAV_LEVELS	5
#define TRANSFORM	cdf97

typedef union {
	struct  {
		unsigned char Quant	:5;
		unsigned char Color	:1;
	};
	char last;
} Header;

void CompressImage(string & infile, string & outfile, int Quant)
{
	Header Head;
	Head.Quant = Quant;
	Head.Color = 0;

	ofstream oFile( outfile.c_str() , ios::out );
	oFile << "RUD2";

	CImg<short> img( infile.c_str() );
	unsigned int imSize = img.dimx() * img.dimy() * img.dimv();
	unsigned char * pStream = new unsigned char[imSize];

	if (Quant == 0)
		img -= 128;
	else
		cimg_for(img, ptr, short) { *ptr = (*ptr - 128) << SHIFT; }

	if (img.dimv() == 3)
		Head.Color = 1;

	unsigned short tmp = img.dimx();
	oFile.write((char *)&tmp, sizeof(unsigned short));
	tmp = img.dimy();
	oFile.write((char *)&tmp, sizeof(unsigned short));
	oFile.write((char *)&Head, sizeof(Header));
	unsigned char * pEnd = pStream;

	CMuxCodec Codec(pEnd, 0);

	CWavelet2D Wavelet(img.dimx(), img.dimy(), WAV_LEVELS);
	Wavelet.SetWeight(TRANSFORM);

	Wavelet.Transform<TRANSFORM>(img.ptr(0,0,0,0), img.dimx());
	Wavelet.CodeBand(&Codec, 1, Quants(Quant + SHIFT * 5), Quants(Quant + SHIFT * 5 - 7));
	if (Head.Color) {
		Wavelet.Transform<TRANSFORM>(img.ptr(0,0,0,1), img.dimx());
		Wavelet.CodeBand(&Codec, 1, Quants(Quant + SHIFT * 5), Quants(Quant + SHIFT * 5 - 7));
		Wavelet.Transform<TRANSFORM>(img.ptr(0,0,0,2), img.dimx());
		Wavelet.CodeBand(&Codec, 1, Quants(Quant + SHIFT * 5), Quants(Quant + SHIFT * 5 - 7));
	}

	pEnd = Codec.endCoding();

	oFile.write((char *) pStream + 2, (pEnd - pStream - 2));
	oFile.close();

	delete[] pStream;
}

void DecompressImage(string & infile, string & outfile, int Dither)
{
	ifstream iFile( infile.c_str() , ios::in );
	char magic[4] = {0,0,0,0};

	iFile.read(magic, 4);

	if (magic[0] != 'R' || magic[1] != 'U' || magic[2]!= 'D' || magic[3] != '2')
		throw BAD_MAGIC;

	unsigned short width, heigth, channels = 1;
	iFile.read((char *) &width, sizeof(unsigned short));
	iFile.read((char *) &heigth, sizeof(unsigned short));

	Header Head;
	iFile.read(&Head.last, 1);
	if (Head.Color == 1)
		channels = 3;

	CImg<short> img( width, heigth, 1, channels );
	unsigned char * pStream = new unsigned char[width * heigth * channels];

	iFile.read((char *) pStream + 2, width * heigth * channels);

	CMuxCodec Codec(pStream);

	CWavelet2D Wavelet(width, heigth, WAV_LEVELS);
	Wavelet.SetWeight(TRANSFORM);

	Wavelet.DecodeBand(&Codec, 1);
	Wavelet.TSUQi<false>(Quants(Head.Quant + SHIFT * 5));
	Wavelet.TransformI<TRANSFORM>(img.ptr() + width * heigth, width);
	if (Head.Color) {
		Wavelet.DecodeBand(&Codec, 1);
		Wavelet.TSUQi<false>(Quants(Head.Quant + SHIFT * 5));
		Wavelet.TransformI<TRANSFORM>(img.ptr() + width * heigth * 2, width);
		Wavelet.DecodeBand(&Codec, 1);
		Wavelet.TSUQi<false>(Quants(Head.Quant + SHIFT * 5));
		Wavelet.TransformI<TRANSFORM>(img.ptr() + width * heigth * 3, width);
	}

	if (Head.Quant == 0)
		img += 128;
	else if (Dither > 0) {
		dither(img.ptr(), img.dimx(), img.dimy());
		if (Head.Color) {
			dither(img.ptr() + width * heigth, img.dimx(), img.dimy());
			dither(img.ptr() + width * heigth * 2, img.dimx(), img.dimy());
		}
	} else
		cimg_for(img, ptr, short) {
			*ptr = 128 + ((*ptr + (1 << (SHIFT - 1))) >> SHIFT);
			*ptr = CLIP(*ptr, 0, 255);
		};

	img.save(outfile.c_str());

	delete[] pStream;
}

/*
void Test(string & infile, int Quant)
{
	Image img( infile );
	img.type( GrayscaleType );
	unsigned int imSize = img.columns() * img.rows();
	short * ImgPixels = new short [imSize * 3];

	img.write(0, 0, img.columns(), img.rows(), "R", CharPixel, ImgPixels);
	for( int i = imSize - 1; i >= 0; i--)
		ImgPixels[i] = (short)((((unsigned char*)ImgPixels)[i] << 4) - (1 << 11));

	CWavelet2D Wavelet(img.columns(), img.rows(), 5);
	Wavelet.SetWeight(TRANSFORM);
	Wavelet.Transform<TRANSFORM>(ImgPixels, img.columns());
	Wavelet.DCT4<true>();
	int count = Wavelet.TSUQ<true>(Quants(Quant), 0.5);

	Wavelet.HBand.GetBand((unsigned char *)ImgPixels);
	BW2RGB((char*)ImgPixels, Wavelet.HBand.DimX * Wavelet.HBand.DimY);
	Image band(Wavelet.HBand.DimXAlign, Wavelet.HBand.DimY, "RGB", CharPixel, ImgPixels);
	band.depth(8);
	band.compressType(UndefinedCompression);
	band.write("/home/nico/Documents/Images/Images test/lena_hband.pgm");

	cout << count << endl;

	Wavelet.TSUQi<true>(Quants(Quant));
	Wavelet.DCT4<false>();
	Wavelet.TransformI<TRANSFORM>(ImgPixels + imSize, img.columns());

	for( unsigned int i = 0; i < imSize; i++) {
		int tmp = ((ImgPixels[i] + (1 << 3)) >> 4) + 128;
		if (tmp > 255) tmp = 255;
		if (tmp < 0) tmp = 0;
		((unsigned char*)ImgPixels)[i] = (char) tmp;
	}

	BW2RGB((char*)ImgPixels, imSize);
	Image img2(img.columns(), img.rows(), "RGB", CharPixel, ImgPixels);
	img2.depth(8);
	img2.compressType(UndefinedCompression);
	img2.write("/home/nico/Documents/Images/Images test/lena_test.pgm");

	delete[] ImgPixels;
}
*/

int main( int argc, char *argv[] )
{
	int c;
	extern char * optarg;
	string infile;
	string outfile;
	int Quant = 9;
	int Type = 0;
	int Dither = 0;

	while ((c = getopt(argc , argv, "i:o:q:t:v:d")) != -1) {
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
			case 'v':
				Type = atoi(optarg);
				break;
			case 'd':
				Dither = 1;
		}
	}
	if (infile.length() == 0) {
		cerr << "An input file name must be specified (option -i)" << endl;
		exit(1);
	}

	cmode mode = encode;
	size_t loc;
	if ((loc = infile.rfind(".ric", string::npos, 4)) != string::npos){
		// decoding
		mode = decode;
		if (outfile.length() == 0) {
			outfile = infile;
// 			outfile.resize(loc);
			outfile.append(".pnm");
		}
	} else {
		// encoding
		if (outfile.length() == 0) {
			outfile = infile;
			loc = infile.find_last_of(".", string::npos, 5);
			size_t loc2 = infile.find_last_of("/");
			if (loc != string::npos && (loc2 < loc || loc2 == string::npos)) {
				outfile.resize(loc);
			}
			outfile.append(".ric");
		}
	}

	if (mode == encode) {
		CompressImage(infile, outfile, Quant);
// 		Test(infile, Quant);
	} else {
		DecompressImage(infile, outfile, Dither);
	}

	return EXIT_SUCCESS;
}
