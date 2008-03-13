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

#include <iostream>
using namespace std;

#include "rududucodec.h"

#define WAV_LEVELS	3
#define TRANSFORM	cdf97
#define BUFFER_SIZE (SUB_IMAGE_CNT + 1)

namespace rududu {

CRududuCodec::CRududuCodec(cmode mode, int width, int height, int component):
	quant(0),
	images(width, height, component, BUFFER_SIZE),
	predImage(0),
	codec(0, 0),
	key_count(0)
{
	wavelet = new CWavelet2D(width, height, WAV_LEVELS);
	wavelet->SetWeight(TRANSFORM);
	if (mode == rududu::encode) {
		obmc = (COBMC*) new COBME(width >> 3, height >> 3);
		predImage = new CImage(width, height, component, ALIGN);
	} else {
		obmc = new COBMC(width >> 3, height >> 3);
		predImage = new CImage(width, height, component, ALIGN);
	}
}


CRududuCodec::~CRududuCodec()
{
	delete predImage;
	delete obmc;
	delete wavelet;
}

short CRududuCodec::quants(int idx)
{
	static const unsigned short Q[5] = {32768, 37641, 43238, 49667, 57052};
	if (idx == 0) return 0; // lossless
	idx--;
	int r = 10 - idx / 5;
	return (short)((Q[idx % 5] + (1 << (r - 1))) >> r );
}

void CRududuCodec::encodeImage(CImage * pImage)
{
	for( int c = 0; c < pImage->component; c++){
		wavelet->Transform(pImage->pImage[c], pImage->dimXAlign, TRANSFORM);
		wavelet->CodeBand(&codec, quants(quant + 20), quants(quant + 12));
		cerr << codec.getSize() << endl;
		wavelet->TSUQi(quants(quant + 20));
		wavelet->TransformI(pImage->pImage[c], pImage->dimXAlign, TRANSFORM);
	}
}

void CRududuCodec::decodeImage(CImage * pImage)
{
	for( int c = 0; c < pImage->component; c++){
		wavelet->DecodeBand(&codec);
		wavelet->TSUQi(quants(quant + 20));
		wavelet->TransformI(pImage->pImage[c], pImage->dimXAlign, TRANSFORM);
	}
}

int CRududuCodec::encode(unsigned char * pImage, int stride, unsigned char * pBuffer, CImage ** outImage)
{
	codec.initCoder(0, pBuffer);

	images.insert(0);
	images[0][0]->inputSGI(pImage, stride, -128);

	if (key_count != 0) {
		COBME * obme = (COBME *) obmc;
		images.calc_sub(1);
		obme->EPZS(images);
		obme->encode(& codec);
		cerr << codec.getSize() << endl;
		obme->apply_mv(images, *predImage);
		*images[0][0] -= *predImage;
		encodeImage(images[0][0]);
		*images[0][0] += *predImage;

		pBuffer[0] |= 0x80;
	} else {
		encodeImage(images[0][0]);
	}

	key_count++;
	if (key_count == 10)
		key_count = 0;

	*outImage = images[0][0];
// 	*outImage = predImage;
	images.remove(1);

	return codec.endCoding() - pBuffer - 2;
}

int CRududuCodec::decode(unsigned char * pBuffer, CImage ** outImage)
{
	codec.initDecoder(pBuffer);

	images.insert(0);

	if (pBuffer[0] & 0x80) {
		images.calc_sub(1);
		obmc->decode(& codec);
		obmc->apply_mv(images, *predImage);
		decodeImage(images[0][0]);
		*images[0][0] += *predImage;
	} else {
		decodeImage(images[0][0]);
	}

	*outImage = images[0][0];
	images.remove(1);

	return codec.getSize();
}

}
