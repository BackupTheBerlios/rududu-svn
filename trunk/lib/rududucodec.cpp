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

#include "rududucodec.h"

#define WAV_LEVELS	5
#define TRANSFORM	cdf97
#define BUFFER_SIZE (SUB_IMAGE_CNT + 1)
// #define DRAW_MV

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

CRududuCodec CRududuCodec::decoder(istream & s)
{
	int width = (s.get() << 8) | s.get();
	int height = (s.get() << 8) | s.get();
	int component = s.get();

	return CRududuCodec(rududu::decode, width, height, component);
}

CRududuCodec::~CRududuCodec()
{
	delete predImage;
	delete obmc;
	delete wavelet;
}

short CRududuCodec::quants(int idx)
{
	static const unsigned short Q[5] = {0x8000, 0x9000, 0xA800, 0xC000, 0xE000};
	if (idx <= 0) return 0; // lossless
	idx--;
	int r = 14 - idx / 5;
	return (short)((Q[idx % 5] + (1 << (r - 1))) >> r );
}

void CRududuCodec::saveParams(ostream & s)
{
	s.put((predImage->dimX >> 8) & 0xFF);
	s.put(predImage->dimX & 0xFF);
	s.put((predImage->dimY >> 8) & 0xFF);
	s.put(predImage->dimY & 0xFF);
	s.put(predImage->component & 0xFF);
}

void CRududuCodec::encodeImage(CImage * pImage)
{
	for( int c = 0; c < pImage->component; c++){
		wavelet->Transform(pImage->pImage[c], pImage->dimXAlign, TRANSFORM);
		wavelet->CodeBand(&codec, quants(quant + 20), quants(quant + 13));
// 		cerr << codec.getSize() << endl;
		wavelet->TSUQi(quants(quant + 20), true);
		wavelet->TransformI(pImage->pImage[c] + pImage->dimXAlign * pImage->dimY,
		                    pImage->dimXAlign, TRANSFORM);
	}
}

void CRududuCodec::decodeImage(CImage * pImage)
{
	for( int c = 0; c < pImage->component; c++){
		wavelet->DecodeBand(&codec);
		wavelet->TSUQi(quants(quant + 20));
		wavelet->TransformI(pImage->pImage[c] + pImage->dimXAlign * pImage->dimY,
		                    pImage->dimXAlign, TRANSFORM);
	}
}

int CRududuCodec::encode(unsigned char * pImage, int stride, unsigned char * pBuffer, CImage ** outImage)
{
	codec.initCoder(0, pBuffer);

	images.insert(0);
	images[0][0]->input(pImage, stride);

	static int f_cnt = 0;

	if (key_count != 0) {
		COBME * obme = (COBME *) obmc;
		images.interpolate(1);
		obme->EPZS(images);
		obme->bt<rududu::encode>(& codec);
// 		obme->global_motion();
// 		char f_name[32];
// 		sprintf(f_name, "/home/nico/f_%03i.ppm", f_cnt);
// 		obme->toppm(f_name);
 		cerr << codec.getSize() << "\t";
		obme->apply_mv(images, *predImage);
		*images[0][0] -= *predImage;
		encodeImage(images[0][0]);
		*images[0][0] += *predImage;

		pBuffer[0] |= 0x80;
	} else {
		encodeImage(images[0][0]);
		cerr << 0 << "\t";
	}
	images.remove(1);

	f_cnt++;

	pBuffer[0] = (pBuffer[0] & 0x80) | quant;

	key_count++;
// 	if (key_count == 10)
// 		key_count = 0;

	int ret = codec.endCoding() - pBuffer;
	cerr << ret;

	if (outImage != 0) {
		*outImage = images[0][0];
		float psnr = (*outImage)->psnr(pImage, stride, 0);
		cerr << "\t" << psnr;
	}
	cerr << endl;

	return ret;
}

int CRududuCodec::decode(unsigned char * pBuffer, CImage ** outImage)
{
	codec.initDecoder(pBuffer);

	quant = pBuffer[0] & 0x7F;

	static int f_cnt = 0;

	images.insert(0);

	if (pBuffer[0] & 0x80) {
		images.interpolate(1);
		obmc->bt<rududu::decode>(& codec);
		obmc->apply_mv(images, *predImage);
		decodeImage(images[0][0]);
		*images[0][0] += *predImage;
	} else {
		decodeImage(images[0][0]);
	}

	f_cnt++;

#ifdef DRAW_MV
	predImage->copy(*images[0][0]);
	if (pBuffer[0] & 0x80)
		obmc->draw_mv(*predImage);
	*outImage = predImage;
#else
	*outImage = images[0][0];
#endif
	images.remove(1);

	return codec.getSize();
}

}
