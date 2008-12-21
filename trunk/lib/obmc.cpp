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

static const short window[8][16] =
{
	{0,	0,	0,	0,	1,	1,	1,	1,	1,	1,	1,	1,	0,	0,	0,	0},
	{0,	0,	1,	1,	1,	2,	2,	2,	2,	2,	2,	1,	1,	1,	0,	0},
	{0,	1,	1,	2,	2,	3,	4,	4,	4,	4,	3,	2,	2,	1,	1,	0},
	{0,	1,	2,	3,	4,	5,	6,	6,	6,	6,	5,	4,	3,	2,	1,	0},
	{1,	1,	2,	4,	5,	7,	8,	9,	9,	8,	7,	5,	4,	2,	1,	1},
	{1,	2,	3,	5,	7,	9,	9,	11,	11,	9,	9,	7,	5,	3,	2,	1},
	{1,	2,	4,	6,	8,	9,	12,	13,	13,	12,	9,	8,	6,	4,	2,	1},
	{1,	2,	4,	6,	9,	11,	13,	14,	14,	13,	11,	9,	6,	4,	2,	1}

// 	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	1,	2,	3,	3,	3,	3,	3,	3,	2,	1,	0,	0,	0},
// 	{0,	0,	1,	2,	4,	5,	6,	6,	6,	6,	5,	4,	2,	1,	0,	0},
// 	{0,	0,	2,	4,	6,	8,	10,	10,	10,	10,	8,	6,	4,	2,	0,	0},
// 	{0,	0,	3,	5,	8,	10,	13,	13,	13,	13,	10,	8,	5,	3,	0,	0},
// 	{0,	0,	3,	6,	10,	13,	16,	16,	16,	16,	13,	10,	6,	3,	0,	0},
// 	{0,	0,	3,	6,	10,	13,	16,	16,	16,	16,	13,	10,	6,	3,	0,	0}

// 	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	16,	16,	16,	16,	16,	16,	16,	16,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	16,	16,	16,	16,	16,	16,	16,	16,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	16,	16,	16,	16,	16,	16,	16,	16,	0,	0,	0,	0},
// 	{0,	0,	0,	0,	16,	16,	16,	16,	16,	16,	16,	16,	0,	0,	0,	0}
};

const pic_lut_t COBMC::qpxl_lut[4 * 4] = {
	{0, -1}, {0, 2}, {2, -1}, {0, 2},
	{0, 1},	 {1, 2}, {2, 3},  {1, 2},
	{1, -1}, {1, 3}, {3, -1}, {1, 3},
	{1, 0},  {1, 2}, {3, 2},  {1, 2}
};

#ifndef __MMX__

static void obmc_block(const short * pSrc, short * pDst,
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

static inline void avg(const short * pSrc1, const short * pSrc2, short * pDst, const int src_stride, const int dst_stride, const int x, const int y)
{
	for (int j = 0; j < y; j++) {
		for (int i = 0; i < x; i++)
			pDst[i] = (pSrc1[i] + pSrc2[i]) >> 1;
		pDst += dst_stride;
		pSrc1 += src_stride;
		pSrc2 += src_stride;
	}
}

#else

typedef short v4hi __attribute__ ((vector_size (8)));

static void obmc_block(const short * pSrc, short * pDst,
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

static inline void avg(const short * pSrc1, const short * pSrc2, short * pDst, const int src_stride, const int dst_stride, int x, int y)
{
	x >>= 2;
	for (int j = 0; j < y; j++) {
		v4hi *src1 = (v4hi *) pSrc1, *src2 = (v4hi *) pSrc2, *dst = (v4hi *) pDst;
		for (int i = 0; i < x; i++) {
			v4hi tmp = __builtin_ia32_paddw(src1[i], src2[i]);
			dst[i] = __builtin_ia32_psraw(tmp, 1);
		}
		pDst += dst_stride;
		pSrc1 += src_stride;
		pSrc2 += src_stride;
	}
}

#endif

template <int flags>
static void obmc_block(const short * pSrc, short * pDst,
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
					pDst[i] = pSrc[i] * (window[j][i] + window[j][i+8]);
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

flatten static void obmc_block(const short * pSrc1, const short * pSrc2,
                               short * pDst, const int src_stride,
                               const int dst_stride)
{
	short tmp[16 * 16]; // FIXME : alignment
	avg(pSrc1, pSrc2, tmp, src_stride, 16, 16, 16);
	obmc_block(tmp, pDst, 16, dst_stride);
}

template <int flags>
static void obmc_block(const short * pSrc1, const short * pSrc2, short * pDst,
                       const int src_stride, const int dst_stride)
{
	int offset = 0, tmp_offset = 0, x = 16, y = 16;

	if (flags & TOP) {
		offset = 4 * src_stride;
		tmp_offset = 16 * 4;
		y = 12;
	}
	if (flags & BOTTOM)
		y -= 4;
	if (flags & LEFT) {
		offset += 4;
		tmp_offset += 4;
		x = 12;
	}
	if (flags & RIGHT)
		x -= 4;

	short tmp[16 * 16]; // FIXME : alignment
	avg(pSrc1 + offset, pSrc2 + offset, tmp + tmp_offset, src_stride, 16, x, y);
	obmc_block<flags>(tmp, pDst, 16, dst_stride);

}

template <int flags>
static short get_block_mean(short * pSrc, const int src_stride)
{
	int s, count = 16 * 6;

	s = sum<short, 8, 8>(pSrc, src_stride);
	s += sum<short, 8, 8>(pSrc + 8, src_stride);
	s += sum<short, 8, 8>(pSrc + 8 * src_stride, src_stride);
	return s / (count * 16);
}

template <int flags>
static void obmc_block_intra(short * pDst, const int dst_stride, const short value)
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

#define OBMC(flags)	\
	{ \
		if (likely(pCurMV[i].all != MV_INTRA)) { \
			sMotionVector v1 = pCurMV[i], v2 = pCurMV[i]; \
			int yx = (v1.x & 3) | ((v1.y & 3) << 2); \
			v1.x++; v2.y++; \
			int src_pos1 = get_pos<16, 4>(v1, i * 8, j * 8, im_x, im_y, stride); \
			if (unlikely(qpxl_lut[yx].pic2 == -1)) { \
				for (int c = 0; c < component; c++) \
					obmc_block<flags>(RefFrames[pCurRef[i] + 1][qpxl_lut[yx].pic1]->pImage[c] + src_pos1, \
								dstImage.pImage[c] + dst_pos, stride, stride); \
			} else { \
				int src_pos2 = get_pos<16, 4>(v2, i * 8, j * 8, im_x, im_y, stride); \
				for (int c = 0; c < component; c++) \
					obmc_block<flags>(RefFrames[pCurRef[i] + 1][qpxl_lut[yx].pic1]->pImage[c] + src_pos1, \
								RefFrames[pCurRef[i] + 1][qpxl_lut[yx].pic2]->pImage[c] + src_pos2, \
								dstImage.pImage[c] + dst_pos, stride, stride); \
			} \
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
			if (likely(pCurMV[i].all != MV_INTRA)) {
				sMotionVector v1 = pCurMV[i], v2 = pCurMV[i];
				int yx = (v1.x & 3) | ((v1.y & 3) << 2);
				v1.x++; v2.y++;
				int src_pos1 = get_pos<16, 4>(v1, i * 8, j * 8, im_x, im_y, stride);
				if (unlikely(qpxl_lut[yx].pic2 == -1)) {
					for (int c = 0; c < component; c++)
						obmc_block(RefFrames[pCurRef[i] + 1][qpxl_lut[yx].pic1]->pImage[c] + src_pos1,
						           dstImage.pImage[c] + dst_pos, stride, stride);
				} else {
					int src_pos2 = get_pos<16, 4>(v2, i * 8, j * 8, im_x, im_y, stride);
					for (int c = 0; c < component; c++)
						obmc_block(RefFrames[pCurRef[i] + 1][qpxl_lut[yx].pic1]->pImage[c] + src_pos1,
						           RefFrames[pCurRef[i] + 1][qpxl_lut[yx].pic2]->pImage[c] + src_pos2,
						           dstImage.pImage[c] + dst_pos, stride, stride);
				}
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

static void draw_line(short * pDst, const int stride, int x1, int y1, int x2, int y2, int value)
{
	int dx = x2 - x1;
	int dy = y2 - y1;

	if (dx == 0 && dy == 0) {
		pDst[x1 + y1 * stride] = clip(pDst[x1 + y1 * stride] + value, -1 << 11, (1 << 11) - 1);
		return;
	}

	if (abs(dx) > abs(dy)) {
     	//handle "horizontal" lines
		if (x2 < x1) {
			int tmp = x1; x1 = x2; x2 = tmp;
			tmp = y1; y1 = y2; y2 = tmp;
		}
		int gradient = ((dy << 16) + (dx >> 1)) / dx;

		short * pCur = pDst + y1 * stride;
		pCur[x1] = clip(pCur[x1] + value, -1 << 11, (1 << 11) - 1);
		int intery = 0;
		for (int x = x1 + 1; x < x2; x++) {
			intery += gradient;
			if (intery < 0) {
				pCur -= stride;
				intery += 1 << 16;
			}
			if (intery > (1 << 16)) {
				pCur += stride;
				intery -= 1 << 16;
			}
			int val = 0;
			val = (value * intery) >> 16;
			pCur[x] = clip(pCur[x] + value - val, -1 << 11, (1 << 11) - 1);
			pCur[x + stride] = clip(pCur[x + stride] + val, -1 << 11, (1 << 11) - 1);
		}
		pCur = pDst + y2 * stride;
		pCur[x2] = clip(pCur[x2] + value, -1 << 11, (1 << 11) - 1);
	} else {
      //handle "vertical" lines
		if (y2 < y1) {
			int tmp = x1; x1 = x2; x2 = tmp;
			tmp = y1; y1 = y2; y2 = tmp;
		}
		int gradient = ((dx << 16) + (dy >> 1)) / dy;

		short * pCur = pDst + y1 * stride;
		short * pEnd = pDst + (y2 - 1) * stride;
		pCur[x1] = clip(pCur[x1] + value, -1 << 11, (1 << 11) - 1);
		int intery = 0;
		pCur += stride;
		for (int x = x1; pCur < pEnd; pCur += stride) {
			intery += gradient;
			if (intery < 0) {
				x--;
				intery += 1 << 16;
			}
			if (intery > (1 << 16)) {
				x++;
				intery -= 1 << 16;
			}
			int val = 0;
			val = (value * intery) >> 16;
			pCur[x] = clip(pCur[x] + value - val, -1 << 11, (1 << 11) - 1);
			pCur[x + 1] = clip(pCur[x + 1] + val, -1 << 11, (1 << 11) - 1);
		}
		pCur[x2] = clip(pCur[x2] + value, -1 << 11, (1 << 11) - 1);
	}
}

void COBMC::draw_mv(CImage & dstImage)
{
	const int stride = dstImage.dimXAlign;
	const int im_x = dstImage.dimX, im_y = dstImage.dimY;
	sMotionVector * pCurMV = pMV;

	for( unsigned int j = 0; j < dimY; j++) {
		for( unsigned int i = 0; i < dimX; i++) {
			if (pCurMV[i].all != MV_INTRA) {
				int x1 = (i << 3) + 4, y1 = (j << 3) + 4;
				int x2 = x1 + pCurMV[i].x, y2 = y1 + pCurMV[i].y;
				if (x2 > 0 && x2 < im_x - 1 && y2 > 0 && y2 < im_y - 1) {
					draw_line(dstImage.pImage[0], stride, x1, y1, x2, y2, 512);
				} else {
					// TODO draw out of frame MV
				}
			} else {
				// TODO draw intra block
			}
		}
		pCurMV += dimX;
	}
}

#define MV_MODEL_CNT (3 * 16)
#define MV_CTX_CNT	4 // number of contexts to code the motion vector difference
#define MV_CTX_SHIFT 1

static const char intra_conv[4][4] = {
	{5},
	{0, 4},
	{0, 2, 4},
	{0, 2, 3, 4}
};

// TODO remove maxCode/maxDecode as they recompute bitlen(max)
static inline void code_mv(sMotionVector mv, sMotionVector * lst, int lst_cnt,
                           CBitCodec & bCodec, CHuffCodec * huff,
                           CMuxCodec * codec)
{
	int intra_ctx = lst_cnt, mv_ctx;
	lst_cnt = COBMC::filter_mv(lst, lst_cnt);

	if (intra_ctx != 4)
		intra_ctx = intra_conv[intra_ctx][intra_ctx - lst_cnt];
	else
		intra_ctx -= lst_cnt;

	if (bCodec.code(mv.all == MV_INTRA, MV_CTX_CNT + intra_ctx))
		return;

	sMotionVector pred;
	if (lst_cnt == 0) {
		pred.all = 0;
		mv_ctx = MV_CTX_CNT - 1; // FIXME arbitrary, default value should be from coding stats
	} else {
		mv_ctx = COBMC::median_mv(lst, lst_cnt);
		if (mv_ctx == -1)
			mv_ctx = MV_CTX_CNT - 1; // FIXME arbitrary, default value should be from coding stats
		else
			mv_ctx = min(MV_CTX_CNT - 1, (bitlen(mv_ctx) + (1 << MV_CTX_SHIFT) - 1) >> MV_CTX_SHIFT);
		pred = lst[0];
	}

	if (bCodec.code(mv.all != pred.all, mv_ctx)) {
		int x = s2u(mv.x - pred.x) + 1;
		int y = s2u(mv.y - pred.y) + 1;

		int lx = bitlen(x >> 1);
		int ly = bitlen(y >> 1);

		int tmp = MAX(lx, ly) - 1;
		if (lx > ly) {
			huff[mv_ctx].code(tmp | (2 << 4), codec);
			codec->bitsCode(x & ~(-1 << lx), lx);
			if (lx > 1)
				codec->maxCode((y - 1) & ~(-1 << lx), (1 << lx) - 2);
		} else if (lx == ly) {
			huff[mv_ctx].code(tmp | (1 << 4), codec);
			codec->bitsCode(x & ~(-1 << lx), lx);
			codec->bitsCode(y & ~(-1 << lx), lx);
		} else {
			huff[mv_ctx].code(tmp, codec);
			if (ly > 1)
				codec->maxCode((x - 1) & ~(-1 << ly), (1 << ly) - 2);
			codec->bitsCode(y & ~(-1 << ly), ly);
		}
	}
}

static inline sMotionVector decode_mv(sMotionVector * lst, int lst_cnt,
                                      CBitCodec & bCodec, CHuffCodec * huff,
                                      CMuxCodec * codec)
{
	int intra_ctx = lst_cnt, mv_ctx;
	lst_cnt = COBMC::filter_mv(lst, lst_cnt);

	if (intra_ctx != 4)
		intra_ctx = intra_conv[intra_ctx][intra_ctx - lst_cnt];
	else
		intra_ctx -= lst_cnt;

	sMotionVector mv;
	if (bCodec.decode(MV_CTX_CNT + intra_ctx)) {
		mv.all = MV_INTRA;
		return mv;
	}

	if (lst_cnt == 0) {
		mv.all = 0;
		mv_ctx = MV_CTX_CNT - 1; // FIXME arbitrary, default value should be from coding stats
	} else {
		mv_ctx = COBMC::median_mv(lst, lst_cnt);
		if (mv_ctx == -1)
			mv_ctx = MV_CTX_CNT - 1; // FIXME arbitrary, default value should be from coding stats
		else
			mv_ctx = min(MV_CTX_CNT - 1, (bitlen(mv_ctx) + (1 << MV_CTX_SHIFT) - 1) >> MV_CTX_SHIFT);
		mv = lst[0];
	}

	if (bCodec.decode(mv_ctx)) {
		int tmp = huff[mv_ctx].decode(codec);
		int l = (tmp & ~(-1 << 4)) + 1;
		tmp >>= 4;
		switch( tmp ){
		case 0:
			if (l > 1)
				mv.x += u2s(codec->maxDecode((1 << l) - 2));
			mv.y += u2s(((1 << l) | codec->bitsDecode(l)) - 1);
			break;
		case 1:
			mv.x += u2s(((1 << l) | codec->bitsDecode(l)) - 1);
			mv.y += u2s(((1 << l) | codec->bitsDecode(l)) - 1);
			break;
		default:
			mv.x += u2s(((1 << l) | codec->bitsDecode(l)) - 1);
			if (l > 1)
				mv.y += u2s(codec->maxDecode((1 << l) - 2));
			break;
		}
	}
	return mv;
}

#define CODE_DECODE_MV(mv)	\
	if (mode == rududu::encode) \
		code_mv(mv, lst, n, bCodec, huff, codec); \
	else { \
		mv = decode_mv(lst, n, bCodec, huff, codec); \
		if (STEP_STOP == 4 && step == 4) \
			pMV[j * dimX + i + 1] = pMV[(j - 1) * dimX + i] = pMV[(j - 1) * dimX + i + 1] = mv; \
	}

#define STEP_STOP 2

/**
 * binary tree coding of the motion vectors
 * something like :
 * http://intuac.com/userport/john/btpc5/
 * @param codec the CMuxCodec where you want to send bits
 */
template <cmode mode>
void COBMC::bt(CMuxCodec * codec)
{
	CBitCodec bCodec(codec);
	char * mem[MV_CTX_CNT * sizeof(CHuffCodec)];
	sMotionVector lst[4];
	CHuffCodec * huff = (CHuffCodec *) mem;
	for(int i = 0; i < MV_CTX_CNT; i++) {
		huff = new(huff) CHuffCodec (mode, 0, MV_MODEL_CNT);
		huff++;
	}
	huff -= MV_CTX_CNT;

	uint step = 8;

	uint line_step = step * dimX;
	sMotionVector * pCurMV = pMV + line_step - dimX;

	for (uint j = step - 1; j < dimY; j += step) {
		for( uint i = 0; i < dimX; i += step) {
			int n = 0;
			if (i > 0)
				lst[n++] = pCurMV[i - step];
			if (j >= step) {
				lst[n++] = pCurMV[i - line_step];
				if (i < dimX - step)
					lst[n++] = pCurMV[i - line_step + step];
				if (i > 0)
					lst[n++] = pCurMV[i - line_step - step];
			}
			CODE_DECODE_MV(pCurMV[i]);
		}
		pCurMV += line_step;
	}

	for (; step >= STEP_STOP; step >>= 1) {
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
				CODE_DECODE_MV(pMV[j * dimX + i]);
			}
		}
		uint istart = 0;
		for (uint j = step_2 - 1; j < dimY; j += step_2) {
			for (uint i = istart; i < dimX; i += step) {
				int n = 0;
				if (i >= step_2)
					lst[n++] = pMV[j * dimX + i - step_2];
				if (i + step_2 < dimX)
					lst[n++] = pMV[j * dimX + i + step_2];
				if (j >= step_2)
					lst[n++] = pMV[(j - step_2) * dimX + i];
				if (j + step_2 < dimX)
					lst[n++] = pMV[(j + step_2) * dimX + i];
				CODE_DECODE_MV(pMV[j * dimX + i]);
			}
			istart = step_2 - istart;
		}
	}
	for (int i = 0; i < MV_CTX_CNT; i++) {
		huff->~CHuffCodec();
		huff++;
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
