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

#include <stdint.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include "bitcodec.h"
#include "huffcodec.h"
#include "obmc.h"
#include "dct2d.h"

using namespace std;

namespace rududu {

COBMC::COBMC(unsigned int dimX, unsigned int dimY):
		pData(0)
{
	this->dimX = dimX;
	this->dimY = dimY;

	int allocated = (dimX * dimY + 1) * (sizeof(sMotionVector) + 1);
	pData = new char[allocated];

	// FIXME : remove this :
	memset(pData, 0, allocated);

	pMV = (sMotionVector*)(((intptr_t)pData + sizeof(sMotionVector) - 1) & (-sizeof(sMotionVector)));
	pRef = (unsigned char*) (pMV + dimX * dimY);
}

COBMC::~COBMC()
{
	delete[] pData;
}

const short COBMC::window[8][16] =
{
	{0,	0,	0,	0,	1,	1,	1,	1,	1,	1,	1,	1,	0,	0,	0,	0},
	{0,	0,	1,	1,	1,	2,	2,	2,	2,	2,	2,	1,	1,	1,	0,	0},
	{0,	1,	1,	2,	2,	3,	4,	4,	4,	4,	3,	2,	2,	1,	1,	0},
	{0,	1,	2,	3,	4,	5,	6,	6,	6,	6,	5,	4,	3,	2,	1,	0},
	{1,	1,	2,	4,	5,	7,	8,	9,	9,	8,	7,	5,	4,	2,	1,	1},
	{1,	2,	3,	5,	7,	9,	9,	11,	11,	9,	9,	7,	5,	3,	2,	1},
	{1,	2,	4,	6,	8,	9,	12,	13,	13,	12,	9,	8,	6,	4,	2,	1},
	{1,	2,	4,	6,	9,	11,	13,	14,	14,	13,	11,	9,	6,	4,	2,	1}
};

// const short COBMC::window[8][8] =
// {
// 	{0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	16,	16,	16,	16},
// 	{0,	0,	0,	0,	16,	16,	16,	16},
// 	{0,	0,	0,	0,	16,	16,	16,	16},
// 	{0,	0,	0,	0,	16,	16,	16,	16}
// };

#ifndef __MMX__
void COBMC::obmc_block(const short * pSrc, short * pDst,
                       const int src_stride, const int dst_stride)
{
	for( int j = 0; j < 8; j++) {
		for( int i = 0; i < 8; i++) {
			pDst[i] += pSrc[i] * window[j][i] + 8;
			pDst[i] >>= 4;
			pDst[i+8] += pSrc[i+8] * window[j][i+8];
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}

	for( int j = 7; j >= 0; j--) {
		for( int i = 0; i < 8; i++) {
			pDst[i] += pSrc[i] * window[j][i];
			pDst[i+8] = pSrc[i+8] * window[j][i+8];
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}
}

#else

typedef short v4hi __attribute__ ((vector_size (8)));

void COBMC::obmc_block(const short * pSrc, short * pDst,
                       const int src_stride, const int dst_stride)
{
	const v4hi rnd = {8, 8, 8, 8 };
	for( int j = 0; j < 8; j++) {
		v4hi * src = (v4hi *) pSrc, * dst = (v4hi *) pDst, * win = (v4hi *) window[j];
		{
			v4hi tmp = __builtin_ia32_pmullw(src[0], win[0]);
			tmp = __builtin_ia32_paddw(tmp, rnd);
			tmp = __builtin_ia32_paddw(tmp, dst[0]);
			dst[0] = __builtin_ia32_psraw(tmp, 4);
		}
		{
			v4hi tmp = __builtin_ia32_pmullw(src[1], win[1]);
			tmp = __builtin_ia32_paddw(tmp, rnd);
			tmp = __builtin_ia32_paddw(tmp, dst[1]);
			dst[1] = __builtin_ia32_psraw(tmp, 4);
		}
		{
			v4hi tmp = __builtin_ia32_pmullw(src[2], win[2]);
			dst[2] = __builtin_ia32_paddw(dst[2], tmp);
		}
		{
			v4hi tmp = __builtin_ia32_pmullw(src[3], win[3]);
			dst[3] = __builtin_ia32_paddw(dst[3], tmp);
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}

	for( int j = 7; j >= 0; j--) {
		v4hi * src = (v4hi *) pSrc, * dst = (v4hi *) pDst, * win = (v4hi *) window[j];
		{
			v4hi tmp = __builtin_ia32_pmullw(src[0], win[0]);
			dst[0] = __builtin_ia32_paddw(dst[0], tmp);
		}
		{
			v4hi tmp = __builtin_ia32_pmullw(src[1], win[1]);
			dst[1] = __builtin_ia32_paddw(dst[1], tmp);
		}
		dst[2] = __builtin_ia32_pmullw(src[2], win[2]);
		dst[3] = __builtin_ia32_pmullw(src[3], win[3]);
		pDst += dst_stride;
		pSrc += src_stride;
	}
}

#endif

template <int flags>
void COBMC::obmc_block(const short * pSrc, short * pDst,
                       const int src_stride, const int dst_stride)
{
	int is = 0, ie = 8, js = 0, je = 0;

	if (flags & TOP) {
		js = 4;
		pDst += 4 * dst_stride;
		pSrc += 4 * src_stride;
	}
	if (flags & BOTTOM)
		je = 4;
	if (flags & LEFT)
		is = 4;
	if (flags & RIGHT)
		ie = 4;

	for( int j = js; j < 8; j++) {
		for( int i = is; i < 8; i++) {
			if (flags & TOP)
				if (flags & LEFT)
					pDst[i] = pSrc[i];
				else
					pDst[i] = (pDst[i] + pSrc[i] * (window[j][i] + window[7-j][i]) + 8) >> 4;
			else
				if (flags & LEFT)
					pDst[i] = (pDst[i] + pSrc[i] * (window[j][i] + window[j][i+8]) + 8) >> 4;
				else
					pDst[i] = (pDst[i] + pSrc[i] * window[j][i] + 8) >> 4;
		}
		for( int i = 0; i < ie; i++) {
			if (flags & TOP)
				if (flags & RIGHT)
					pDst[i+8] = pSrc[i+8];
				else
					pDst[i+8] = pSrc[i+8] * (window[j][i+8] + window[7-j][i+8]);
			else
				if (flags & RIGHT)
					pDst[i+8] = (pDst[i+8] + pSrc[i+8] * (window[j][i+8] + window[j][i]) + 8) >> 4;
				else
					pDst[i+8] += pSrc[i+8] * window[j][i+8];
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}

	for( int j = 7; j >= je; j--) {
		for( int i = is; i < 8; i++) {
			if (flags & BOTTOM)
				if (flags & LEFT)
					pDst[i] = pSrc[i];
				else
					pDst[i] = (pDst[i] + pSrc[i] * (window[j][i] + window[7-j][i]) + 8) >> 4;
			else
				if (flags & LEFT)
					pDst[i] += pSrc[i] * (window[j][i] + window[j][i+8]);
				else
					pDst[i] += pSrc[i] * window[j][i];
		}
		for( int i = 0; i < ie; i++) {
			if (flags & BOTTOM)
				if (flags & RIGHT)
					pDst[i+8] = pSrc[i+8];
				else
					pDst[i+8] = pSrc[i+8] * (window[j][i+8] + window[7-j][i+8]);
			else
				if (flags & RIGHT)
					pDst[i+8] = pSrc[i+8] * (window[j][i+8] + window[j][i]);
				else
					pDst[i+8] = pSrc[i+8] * window[j][i+8];
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}
}

template <int flags>
short COBMC::get_block_mean(short * pSrc, const int src_stride)
{
	int s, count = 16 * 6;

	s = sum<short, 8, 8>(pSrc, src_stride);
	s += sum<short, 8, 8>(pSrc + 8, src_stride);
	s += sum<short, 8, 8>(pSrc + 8 * src_stride, src_stride);
	return s / (count * 16);
}

template <int flags>
void COBMC::obmc_block_intra(short * pDst, const int dst_stride, const short value)
{
	int is = 0, ie = 8, js = 0, je = 0;

	if (flags & TOP) {
		js = 4;
		pDst += 4 * dst_stride;
	}
	if (flags & BOTTOM)
		je = 4;
	if (flags & LEFT)
		is = 4;
	if (flags & RIGHT)
		ie = 4;

	for( int j = js; j < 8; j++) {
		for( int i = is; i < 8; i++) {
			if (flags & TOP)
				if (flags & LEFT)
					pDst[i] = value;
			else
				pDst[i] = (pDst[i] + value * (window[j][i] + window[7-j][i]) + 8) >> 4;
			else
				if (flags & LEFT)
					pDst[i] = (pDst[i] + value * (window[j][i] + window[j][7-i]) + 8) >> 4;
			else
				pDst[i] = (pDst[i] + value * window[j][i] + 8) >> 4;
		}
		for( int i = 0; i < ie; i++) {
			if (flags & TOP)
				if (flags & RIGHT)
					pDst[i+8] = value;
			else
				pDst[i+8] = value * (window[j][7-i] + window[7-j][7-i]);
			else
				if (flags & RIGHT)
					pDst[i+8] = (pDst[i+8] + value * (window[j][7-i] + window[j][i]) + 8) >> 4;
			else
				pDst[i+8] += value * window[j][7-i];
		}
		pDst += dst_stride;
	}

	for( int j = 7; j >= je; j--) {
		for( int i = is; i < 8; i++) {
			if (flags & BOTTOM)
				if (flags & LEFT)
					pDst[i] = value;
			else
				pDst[i] = (pDst[i] + value * (window[j][i] + window[7-j][i]) + 8) >> 4;
			else
				if (flags & LEFT)
					pDst[i] += value * (window[j][i] + window[j][7-i]);
			else
				pDst[i] += value * window[j][i];
		}
		for( int i = 0; i < ie; i++) {
			if (flags & BOTTOM)
				if (flags & RIGHT)
					pDst[i+8] = value;
			else
				pDst[i+8] = value * (window[j][7-i] + window[7-j][7-i]);
			else
				if (flags & RIGHT)
					pDst[i+8] = value * (window[j][7-i] + window[j][i]);
			else
				pDst[i+8] = value * window[j][7-i];
		}
		pDst += dst_stride;
	}
}

int COBMC::get_pos(const sMotionVector mv, const unsigned int i,
                    const unsigned int j, const unsigned int im_x,
                    const unsigned int im_y, const int stride)
{
	int x = i * 8 + (mv.x >> 2) - 4;
	int y = j * 8 + (mv.y >> 2) - 4;
	if (x < -15) x = -15;
	if (x >= (int)im_x) x = im_x - 1;
	if (y < -15) y = -15;
	if (y >= (int)im_y) y = im_y - 1;
	return x + y * stride;
}

#define OBMC(flags)	\
	{ \
		if (pCurMV[i].all != MV_INTRA) { \
			int src_pos = get_pos(pCurMV[i], i, j, im_x, im_y, stride); \
			int pic = ((pCurMV[i].x & 3) << 2) | (pCurMV[i].y & 3); \
			for( int c = 0; c < component; c++) \
				obmc_block<flags>(RefFrames[pCurRef[i] + 1][pic]->pImage[c] + src_pos, \
					dstImage.pImage[c] + dst_pos, stride, stride); \
		} else \
			for( int c = 0; c < component; c++) \
				obmc_block_intra<flags>(dstImage.pImage[c] + dst_pos, stride, 0); \
	}

void COBMC::apply_mv(CImageBuffer & RefFrames, CImage & dstImage)
{
	const int stride = dstImage.dimXAlign;
	const int diff = dstImage.dimXAlign * 8 - dimX * 8 + 8;
	const int im_x = dstImage.dimX, im_y = dstImage.dimY;
	sMotionVector * pCurMV = pMV;
	unsigned char * pCurRef = pRef;
	int dst_pos = -4 - 4 * stride, component = dstImage.component;

	unsigned int j = 0, i = 0;

	OBMC(TOP | LEFT);
	dst_pos += 8;
	for( i = 1; i < dimX - 1; i++) {
		OBMC(TOP);
		dst_pos += 8;
	}
	OBMC(TOP | RIGHT);

	pCurMV += dimX;
	pCurRef += dimX;
	dst_pos += diff;

	for( j = 1; j < dimY - 1; j++) {
		i = 0;
		OBMC(LEFT);
		dst_pos += 8;
		for( i = 1; i < dimX - 1; i++) {
			if (pCurMV[i].all != MV_INTRA) {
				int src_pos = get_pos(pCurMV[i], i, j, im_x, im_y, stride);
				int pic = ((pCurMV[i].x & 3) << 2) | (pCurMV[i].y & 3);
				for( int c = 0; c < component; c++)
					obmc_block(RefFrames[pCurRef[i] + 1][pic]->pImage[c] + src_pos,
					           dstImage.pImage[c] + dst_pos, stride, stride);
			} else
				for( int c = 0; c < component; c++)
					obmc_block_intra<0>(dstImage.pImage[c] + dst_pos, stride, get_block_mean<0>(dstImage.pImage[c] + dst_pos, stride));
			dst_pos += 8;
		}
		OBMC(RIGHT);

		pCurMV += dimX;
		pCurRef += dimX;
		dst_pos += diff;
	}

	i = 0;
	OBMC(BOTTOM | LEFT);
	dst_pos += 8;
	for( i = 1; i < dimX - 1; i++) {
		OBMC(BOTTOM);
		dst_pos += 8;
	}
	OBMC(BOTTOM | RIGHT);

#ifdef __MMX__
	__builtin_ia32_emms();
#endif
}

#define MV_TABLE_SIZE 127u

const sHuffRL huff_mv_x_rl[] = {
	{2, 1}, {-1, 1}, {3, 1}, {1, 2}, {1, 2}, {1, 4}, {1, 4}, {1, 6}, {1, 8}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 5}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 2}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 9}, {1, 22}, {1, 1}, {-1, 3}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 24}, {-4, 1}
};

const sHuffRL huff_mv_y_rl[] = {
	{1, 1}, {1, 1}, {2, 2}, {2, 3}, {1, 4}, {1, 3}, {-1, 1}, {2, 6}, {1, 5}, {1, 1}, {-1, 1}, {1, 6}, {1, 6}, {1, 1}, {-1, 1}, {1, 11}, {1, 1}, {-1, 2}, {1, 14}, {1, 5}, {1, 1}, {-1, 2}, {1, 4}, {-1, 1}, {1, 1}, {-1, 3}, {1, 29}, {-1, 1}, {1, 9}, {-6, 1}
};

static inline void code_mv(sMotionVector mv, sMotionVector pred,
                           CBitCodec & zeroCodec, CHuffCodec & huff,
                           CHuffCodec & huff_x, CHuffCodec & huff_y,
                           CMuxCodec * codec)
{
	if (mv.x == pred.x && mv.y == pred.y)
		zeroCodec.code0(0);
	else {
		zeroCodec.code1(0);
		int x = s2u(mv.x - pred.x);
		int y = s2u(mv.y - pred.y);
		int tmp = (MIN(x, 15) | (MIN(y, 15) << 4)) - 1;
		huff.code(tmp, codec);
		if (x >= 15) {
			huff_x.code(MIN(x - 15, 127), codec);
			if (x >= 127 + 15)
				codec->golombLinCode(x - 127 - 15, 5, 0);
		}
		if (y >= 15) {
			huff_y.code(MIN(y - 15, 127), codec);
			if (y >= 127 + 15)
				codec->golombLinCode(y - 127 - 15, 5, 0);
		}
	}
}

static inline sMotionVector decode_mv(sMotionVector pred, CBitCodec & zeroCodec,
                                      CHuffCodec & huff, CHuffCodec & huff_x,
                                      CHuffCodec & huff_y, CMuxCodec * codec)
{
	sMotionVector mv;
	if (zeroCodec.decode(0)) {
		int tmp = huff.decode(codec) + 1;
		int x = tmp & 0xF;
		int y = tmp >> 4;
		if (x == 15) {
			x += huff_x.decode(codec);
			if (x == 127 + 15)
				x += codec->golombLinDecode(5, 0);
		}
		mv.x = u2s(x) + pred.x;
		if (y == 15) {
			y += huff_y.decode(codec);
			if (y == 127 + 15)
				y += codec->golombLinDecode(5, 0);
		}
		mv.y = u2s(y) + pred.y;
	} else {
		mv.x = pred.x;
		mv.y = pred.y;
	}
	return mv;
}

void COBMC::encode(CMuxCodec * codec)
{
	sMotionVector * pCurMV = pMV;
	CBitCodec intraCodec(codec), zeroCodec(codec);
	CHuffCodec huff_x(rududu::encode, 0, 128);
	CHuffCodec huff_y(rududu::encode, 0, 128);
	CHuffCodec huff(rududu::encode, 0, 255);

	for( unsigned int j = 0; j < dimY; j++) {
		for( unsigned int i = 0; i < dimX; i++) {
			if (pCurMV[i].all == MV_INTRA)
				intraCodec.code1(0);
			else {
				intraCodec.code0(0);
				sMotionVector MVPred = {0};
				if (j == 0) {
					if (i != 0)
						MVPred = pCurMV[i - 1];
				} else {
					if (i == 0 || i == dimX - 1)
						MVPred = pCurMV[i - dimX];
					else {
						sMotionVector tmp[4] = {pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1], pCurMV[i - dimX - 1]};
						MVPred = median_mv(tmp, 3);
					}
				}
				code_mv(pCurMV[i], MVPred, zeroCodec, huff, huff_x, huff_y, codec);
			}
		}
		pCurMV += dimX;
	}
}

void COBMC::decode(CMuxCodec * codec)
{
	sMotionVector * pCurMV = pMV;
	CBitCodec intraCodec(codec), zeroCodec(codec);
	CHuffCodec huff_x(rududu::decode, 0, 128);
	CHuffCodec huff_y(rududu::decode, 0, 128);
	CHuffCodec huff(rududu::decode, 0, 255);

	for( unsigned int j = 0; j < dimY; j++) {
		for( unsigned int i = 0; i < dimX; i++) {
			if (intraCodec.decode(0))
				pCurMV[i].all = MV_INTRA;
			else {
				sMotionVector MVPred = {0};
				if (j == 0) {
					if (i != 0)
						MVPred = pCurMV[i - 1];
				} else {
					if (i == 0 || i == dimX - 1)
						MVPred = pCurMV[i - dimX];
					else {
						sMotionVector tmp[4] = {pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1], pCurMV[i - dimX - 1]};
						MVPred = median_mv(tmp, 3);
					}
				}
				pCurMV[i] = decode_mv( MVPred, zeroCodec, huff, huff_x, huff_y, codec);
			}
		}
		pCurMV += dimX;
	}
}

template <cmode mode>
void COBMC::bt(CMuxCodec * codec)
{
	CBitCodec intraCodec(codec), zeroCodec(codec);
	CHuffCodec huff_x(mode, 0, 128);
	CHuffCodec huff_y(mode, 0, 128);
	CHuffCodec huff(mode, 0, 255);
	sMotionVector lst[4];

	uint step = 8;

	uint line_step = step * dimX;
	sMotionVector * pCurMV = pMV + line_step - dimX;

	for( uint j = step - 1; j < dimY; j += step) {
		for( uint i = 0; i < dimX; i += step) {
			if ((mode == rududu::encode && intraCodec.code(pCurMV[i].all == MV_INTRA, 0)) || (mode == rududu::decode &&  intraCodec.decode(0))) {
				if (mode == rududu::decode) pCurMV[i].all = MV_INTRA;
			} else {
				sMotionVector MVPred = {0};
				if (j < step) {
					if (i > 0)
						MVPred = pCurMV[i - step];
				} else {
					if (i == 0 || i >= dimX - step)
						MVPred = pCurMV[i - line_step];
					else {
						sMotionVector tmp[3] = {pCurMV[i - step], pCurMV[i - line_step], pCurMV[i - line_step + step]};
						MVPred = median_mv(tmp, 3);
					}
				}
				if (mode == rududu::encode)
					code_mv(pCurMV[i], MVPred, zeroCodec, huff, huff_x, huff_y, codec);
				else
					pCurMV[i] = decode_mv(MVPred, zeroCodec, huff, huff_x, huff_y, codec);
			}
		}
		pCurMV += line_step;
	}

	for( ; step >= 2; step >>= 1){
		uint step_2 = step >> 1;
		for( uint j = step_2 - 1; j < dimY; j += step){
			for( uint i = step_2; i < dimX; i += step){
				int n = 0;
				if (j >= step_2) {
					if (i >= step_2)
						lst[n++] = pMV[(j - step_2) * dimX + i - step_2];
					if (i + step_2 < dimX)
						lst[n++] = pMV[(j - step_2) * dimX + i + step_2];
				}
				if (j + step_2 < dimY) {
					if (i >= step_2)
						lst[n++] = pMV[(j + step_2) * dimX + i - step_2];
					if (i + step_2 < dimX)
						lst[n++] = pMV[(j + step_2) * dimX + i + step_2];
				}
				if ((mode == rududu::encode && intraCodec.code(pMV[j * dimX + i].all == MV_INTRA, 0)) || (mode == rududu::decode &&  intraCodec.decode(0))) {
					if (mode == rududu::decode) pMV[j * dimX + i].all = MV_INTRA;
				} else {
					sMotionVector pred = median_mv(lst, n);
					if (mode == rududu::encode)
						code_mv(pMV[j * dimX + i], pred, zeroCodec, huff, huff_x, huff_y, codec);
					else
						pMV[j * dimX + i] = decode_mv(pred, zeroCodec, huff, huff_x, huff_y, codec);
				}
			}
		}
		uint istart = 0;
		for( uint j = step_2 - 1; j < dimY; j += step_2){
			for( uint i = istart; i < dimX; i += step){
				int n = 0;
				if (i >= step_2)
					lst[n++] = pMV[j * dimX + i - step_2];
				if (i + step_2 < dimX)
					lst[n++] = pMV[j * dimX + i + step_2];
				if (j >= step_2)
					lst[n++] = pMV[(j - step_2) * dimX + i];
				if (j + step_2 < dimX)
					lst[n++] = pMV[(j + step_2) * dimX + i];
				if ((mode == rududu::encode && intraCodec.code(pMV[j * dimX + i].all == MV_INTRA, 0)) || (mode == rududu::decode &&  intraCodec.decode(0))) {
					if (mode == rududu::decode) pMV[j * dimX + i].all = MV_INTRA;
				} else {
					sMotionVector pred = median_mv(lst, n);
					if (mode == rududu::encode)
						code_mv(pMV[j * dimX + i], pred, zeroCodec, huff, huff_x, huff_y, codec);
					else
						pMV[j * dimX + i] = decode_mv(pred, zeroCodec, huff, huff_x, huff_y, codec);
				}
			}
			istart = step_2 - istart;
		}
	}
}

template void COBMC::bt<rududu::encode>(CMuxCodec * codec);
template void COBMC::bt<rududu::decode>(CMuxCodec * codec);

void COBMC::toppm(char * file_name) {
	ofstream f(file_name, ios_base::out | ios_base::binary | ios_base::trunc);
	f << "P6" << endl << dimX << " " << dimY << endl << "255" << endl;

	sMotionVector const * pCurMV = pMV;
	for( unsigned int j = 0; j < dimY; j++){
		for( unsigned int i = 0; i < dimX; i++){
			int r = pCurMV[i].x + 128;
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			int g = pCurMV[i].y + 128;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			f.put((unsigned char) r);
			f.put((unsigned char) g);
			f.put(128);
		}
		pCurMV += dimX;
	}

	f.close();
}

}
