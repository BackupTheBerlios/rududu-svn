/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti   *
 *   rududu@laposte.net   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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

#include "rududucodec.h"

#define ALIGN	32
#define WAV_LEVELS	3
#define TRANSFORM	cdf97
#define THRES_RATIO	.7f

namespace rududu {

CRududuCodec::CRududuCodec(cmode mode, int width, int height, int component):
	quant(0),
	predImage(0),
	codec(0, 0),
	key_count(0)
{
	images[0] = new CImage(width, height, component, ALIGN);
	images[1] = new CImage(width, height, component, ALIGN);
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
	delete images[0];
	delete images[1];
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
		wavelet->Transform<TRANSFORM>(pImage->pImage[c], pImage->dimXAlign);
		wavelet->TSUQ(quants(quant), THRES_RATIO);
		wavelet->CodeBand(&codec, 1);
		wavelet->TSUQi(quants(quant));
		wavelet->TransformI<TRANSFORM>(pImage->pImage[c], pImage->dimXAlign);
	}
}

void CRududuCodec::decodeImage(CImage * pImage)
{
	for( int c = 0; c < pImage->component; c++){
		wavelet->DecodeBand(&codec, 1);
		wavelet->TSUQi(quants(quant));
		wavelet->TransformI<TRANSFORM>(pImage->pImage[c], pImage->dimXAlign);
	}
}

int CRududuCodec::encode(unsigned char * pImage, int stride, unsigned char * pBuffer, CImage ** outImage)
{
	codec.initCoder(0, pBuffer);

	images[0]->inputSGI(pImage, stride, -128);

	if (key_count != 0) {
		COBME * obme = (COBME *) obmc;
		images[1]->extend();
		obme->EPZS(images);
		obme->encode(& codec);
		obme->apply_mv(images[1], *predImage);
		*images[0] -= *predImage;
		encodeImage(images[0]);
		*images[0] += *predImage;

		pBuffer[0] |= 0x80;
	} else {
		encodeImage(images[0]);
	}

	key_count++;
	if (key_count == 10)
		key_count = 0;

	*outImage = images[0];
	images[0] = images[1];
	images[1] = *outImage;

	return codec.endCoding() - pBuffer - 2;
}

int CRududuCodec::decode(unsigned char * pBuffer, CImage ** outImage)
{
	codec.initDecoder(pBuffer);

	if (pBuffer[0] & 0x80) {
		images[1]->extend();
		obmc->decode(& codec);
		obmc->apply_mv(images[1], *predImage);
		decodeImage(images[0]);
		*images[0] += *predImage;
	} else {
		decodeImage(images[0]);
	}

	*outImage = images[0];
	images[0] = images[1];
	images[1] = *outImage;

	return codec.getSize();
}

}
