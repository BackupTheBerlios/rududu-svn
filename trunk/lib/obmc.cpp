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

#include <stdint.h>
#include <string.h>

#include "bitcodec.h"
#include "obmc.h"

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

const short COBMC::window[8][8] =
{
	{0,	0,	0,	0,	1,	1,	1,	1},
	{0,	0,	1,	1,	1,	2,	2,	2},
	{0,	1,	1,	2,	2,	3,	4,	4},
	{0,	1,	2,	3,	4,	5,	6,	6},
	{1,	1,	2,	4,	5,	7,	8,	9},
	{1,	2,	3,	5,	7,	9,	9,	11},
	{1,	2,	4,	6,	8,	9,	12,	13},
	{1,	2,	4,	6,	9,	11,	13,	14}
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

void COBMC::obmc_block(const short * pSrc, short * pDst, const int stride)
{
	for( int j = 0; j < 8; j++) {
		for( int i = 0; i < 8; i++) {
			pDst[i] = (pDst[i] + pSrc[i] * window[j][i] + 8) >> 4;
			pDst[i+8] += pSrc[i+8] * window[j][7-i];
		}
		pDst += stride;
		pSrc += stride;
	}

	for( int j = 7; j >= 0; j--) {
		for( int i = 0; i < 8; i++) {
			pDst[i] += pSrc[i] * window[j][i];
			pDst[i+8] = pSrc[i+8] * window[j][7-i];
		}
		pDst += stride;
		pSrc += stride;
	}
}

template <int flags>
void COBMC::obmc_block(const short * pSrc, short * pDst, const int stride)
{
	int is = 0, ie = 8, js = 0, je = 0;

	if (flags & TOP) {
		js = 4;
		pDst += 4 * stride;
		pSrc += 4 * stride;
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
					pDst[i] = (pDst[i] + pSrc[i] * (window[j][i] + window[j][7-i]) + 8) >> 4;
				else
					pDst[i] = (pDst[i] + pSrc[i] * window[j][i] + 8) >> 4;
		}
		for( int i = 0; i < ie; i++) {
			if (flags & TOP)
				if (flags & RIGHT)
					pDst[i+8] = pSrc[i+8];
				else
					pDst[i+8] = pSrc[i+8] * (window[j][7-i] + window[7-j][7-i]);
			else
				if (flags & RIGHT)
					pDst[i+8] = (pDst[i+8] + pSrc[i+8] * (window[j][7-i] + window[j][i]) + 8) >> 4;
				else
					pDst[i+8] += pSrc[i+8] * window[j][7-i];
		}
		pDst += stride;
		pSrc += stride;
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
					pDst[i] += pSrc[i] * (window[j][i] + window[j][7-i]);
				else
					pDst[i] += pSrc[i] * window[j][i];
		}
		for( int i = 0; i < ie; i++) {
			if (flags & BOTTOM)
				if (flags & RIGHT)
					pDst[i+8] = pSrc[i+8];
				else
					pDst[i+8] = pSrc[i+8] * (window[j][7-i] + window[7-j][7-i]);
			else
				if (flags & RIGHT)
					pDst[i+8] = pSrc[i+8] * (window[j][7-i] + window[j][i]);
				else
					pDst[i+8] = pSrc[i+8] * window[j][7-i];
		}
		pDst += stride;
		pSrc += stride;
	}
}

#define CHECK_MV(mv)	\
{	\
	int x = i * 8 + mv.x - 4;	\
	int y = j * 8 + mv.y - 4;	\
	if (x < -15) x = -15;	\
	if (x >= im_x) x = im_x - 1;	\
	if (y < -15) y = -15;	\
	if (y >= im_y) y = im_y - 1;	\
	src_pos = x + y * stride; \
}

void COBMC::apply_mv(CImage * pRefFrames, CImage & dstImage)
{
	const int stride = dstImage.dimXAlign;
	const int diff = dstImage.dimXAlign * 8 - dimX * 8 + 8;
	const int im_x = dstImage.dimX, im_y = dstImage.dimY;
	sMotionVector * pCurMV = pMV;
	unsigned char * pCurRef = pRef;
	int dst_pos = -4 - 4 * stride, src_pos, component = dstImage.component;

	unsigned int j = 0, i = 0;

	CHECK_MV(pCurMV[i]);
	for( int c = 0; c < component; c++)
		obmc_block<TOP | LEFT>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
	dst_pos += 8;
	for( i = 1; i < dimX - 1; i++) {
		CHECK_MV(pCurMV[i]);
		for( int c = 0; c < component; c++)
			obmc_block<TOP>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
		dst_pos += 8;
	}
	CHECK_MV(pCurMV[i]);
	for( int c = 0; c < component; c++)
		obmc_block<TOP | RIGHT>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);

	pCurMV += dimX;
	pCurRef += dimX;
	dst_pos += diff;

	for( j = 1; j < dimY - 1; j++) {
		i = 0;
		CHECK_MV(pCurMV[i]);
		for( int c = 0; c < component; c++)
			obmc_block<LEFT>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
		dst_pos += 8;
		for( i = 1; i < dimX - 1; i++) {
			CHECK_MV(pCurMV[i]);
			for( int c = 0; c < component; c++)
				obmc_block(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
			dst_pos += 8;
		}
		CHECK_MV(pCurMV[i]);
		for( int c = 0; c < component; c++)
			obmc_block<RIGHT>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);

		pCurMV += dimX;
		pCurRef += dimX;
		dst_pos += diff;
	}

	i = 0;
	CHECK_MV(pCurMV[i]);
	for( int c = 0; c < component; c++)
		obmc_block<BOTTOM | LEFT>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
	dst_pos += 8;
	for( i = 1; i < dimX - 1; i++) {
		CHECK_MV(pCurMV[i]);
		for( int c = 0; c < component; c++)
			obmc_block<BOTTOM>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
		dst_pos += 8;
	}
	CHECK_MV(pCurMV[i]);
	for( int c = 0; c < component; c++)
		obmc_block<BOTTOM | RIGHT>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride);
}

#undef CHECK_MV

void COBMC::encode(CMuxCodec * outCodec)
{
	sMotionVector * pCurMV = pMV;
	CBitCodec intraCodec(outCodec);

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
						MVPred = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1]);
					}
				}
				outCodec->tabooCode(s2u(pCurMV[i].x - MVPred.x));
				outCodec->tabooCode(s2u(pCurMV[i].y - MVPred.y));
			}
		}
		pCurMV += dimX;
	}
}

void COBMC::decode(CMuxCodec * inCodec)
{
	sMotionVector * pCurMV = pMV;
	CBitCodec intraCodec(inCodec);

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
						MVPred = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1]);
					}
				}
				pCurMV[i].x = u2s(inCodec->tabooDecode()) + MVPred.x;
				pCurMV[i].y = u2s(inCodec->tabooDecode()) + MVPred.y;
			}
		}
		pCurMV += dimX;
	}
}

}
