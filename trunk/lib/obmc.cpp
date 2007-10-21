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

#include <iostream>

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

void COBMC::obmc_block(const short * pSrc, short * pDst,
                       const int src_stride, const int dst_stride)
{
	for( int j = 0; j < 8; j++) {
		for( int i = 0; i < 8; i++) {
			pDst[i] = (pDst[i] + pSrc[i] * window[j][i] + 8) >> 4;
			pDst[i+8] += pSrc[i+8] * window[j][7-i];
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}

	for( int j = 7; j >= 0; j--) {
		for( int i = 0; i < 8; i++) {
			pDst[i] += pSrc[i] * window[j][i];
			pDst[i+8] = pSrc[i+8] * window[j][7-i];
		}
		pDst += dst_stride;
		pSrc += src_stride;
	}
}

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
		pDst += dst_stride;
		pSrc += src_stride;
	}
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

// [0][1][2]
// [3][ ][4]
// [5][6][7]

template <int flags>
void COBMC::intra_block(short * block[8], short * pDst, const int stride)
{
	short tmp[16][16];
	int cnt = 0;
	// FIXME : memset only for testing
	memset(tmp, 0, sizeof(tmp));

	if (block[1] != 0) {
		copy<short,8,8,false>(block[1] + 8 * stride, &tmp[0][0], stride, 16);
		cnt++;
	}
	if (block[3] != 0) {
		if (cnt == 0)
			copy<short,8,8,false>(block[3] + 8, &tmp[0][0], stride, 16);
		else
			copy<short,8,8,true>(block[3] + 8, &tmp[0][0], stride, 16);
		cnt++;
	}
	if (cnt == 0 && block[0] != 0)
		copy<short,8,8,false>(block[0] + 8 + 8 * stride, &tmp[0][0], stride, 16);

	if (cnt == 2)
		for (int j = 0; j < 8; j++)
			for(int i = 0; i < 8; i++)
				tmp[j][i] >>= 1;

	cnt = 0;
	if (block[1] != 0) {
		copy<short,8,8,false>(block[1] + 8 + 8 * stride, &tmp[0][8], stride, 16);
		cnt++;
	}
	if (block[4] != 0) {
		if (cnt == 0)
			copy<short,8,8,false>(block[4], &tmp[0][8], stride, 16);
		else
			copy<short,8,8,true>(block[4], &tmp[0][8], stride, 16);
		cnt++;
	}
	if (cnt == 0 && block[2] != 0)
		copy<short,8,8,false>(block[2] + 8 * stride, &tmp[0][8], stride, 16);

	if (cnt == 2)
		for (int j = 0; j < 8; j++)
			for(int i = 8; i < 16; i++)
				tmp[j][i] >>= 1;

	cnt = 0;
	if (block[6] != 0) {
		copy<short,8,8,false>(block[6], &tmp[8][0], stride, 16);
		cnt++;
	}
	if (block[3] != 0) {
		if (cnt == 0)
			copy<short,8,8,false>(block[3] + 8 + 8 * stride, &tmp[8][0], stride, 16);
		else
			copy<short,8,8,true>(block[3] + 8 + 8 * stride, &tmp[8][0], stride, 16);
		cnt++;
	}
	if (cnt == 0 && block[5] != 0)
		copy<short,8,8,false>(block[5] + 8, &tmp[8][0], stride, 16);

	if (cnt == 2)
		for (int j = 8; j < 16; j++)
			for(int i = 0; i < 8; i++)
				tmp[j][i] >>= 1;

	cnt = 0;
	if (block[6] != 0) {
		copy<short,8,8,false>(block[6] + 8, &tmp[8][8], stride, 16);
		cnt++;
	}
	if (block[4] != 0) {
		if (cnt == 0)
			copy<short,8,8,false>(block[4] + 8 * stride, &tmp[8][8], stride, 16);
		else
			copy<short,8,8,true>(block[4] + 8 * stride, &tmp[8][8], stride, 16);
		cnt++;
	}
	if (cnt == 0 && block[7] != 0)
		copy<short,8,8,false>(block[7], &tmp[8][8], stride, 16);

	if (cnt == 2)
		for (int j = 8; j < 16; j++)
			for(int i = 8; i < 16; i++)
				tmp[j][i] >>= 1;

	if (flags == 0)
		obmc_block(&tmp[0][0], pDst, 16, stride);
	else
		obmc_block<flags>(&tmp[0][0], pDst, 16, stride);
}

int COBMC::get_pos(const sMotionVector mv, const unsigned int i,
                    const unsigned int j, const unsigned int im_x,
                    const unsigned int im_y, const int stride)
{
	int x = i * 8 + mv.x - 4;
	int y = j * 8 + mv.y - 4;
	if (x < -15) x = -15;
	if (x >= (int)im_x) x = im_x - 1;
	if (y < -15) y = -15;
	if (y >= (int)im_y) y = im_y - 1;
	return x + y * stride;
}

template <int flags>
void COBMC::intra_block(sMotionVector * pCurMV, unsigned char * pCurRef,
                        CImage * pRefFrames,  CImage & dstImage,
                        const unsigned int i, const unsigned int j)
{
	int pos[8] = {0, 0, 0, 0, 0, 0, 0, 0}, component = dstImage.component;
	short * list[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	const int stride = dstImage.dimXAlign;
	const int im_x = dstImage.dimX, im_y = dstImage.dimY;


	if ((flags & (TOP | LEFT)) == 0 && pCurMV[-1-dimX].all != MV_INTRA)
		pos[0] = get_pos(pCurMV[-1-dimX], i - 1, j - 1, im_x, im_y, stride);
	if ((flags & (TOP)) == 0 && pCurMV[-dimX].all != MV_INTRA)
		pos[1] = get_pos(pCurMV[-dimX], i, j - 1, im_x, im_y, stride);
	if ((flags & (TOP | RIGHT)) == 0 && pCurMV[1-dimX].all != MV_INTRA)
		pos[2] = get_pos(pCurMV[1-dimX], i + 1, j - 1, im_x, im_y, stride);
	if ((flags & (LEFT)) == 0 && pCurMV[-1].all != MV_INTRA)
		pos[3] = get_pos(pCurMV[-1], i - 1, j, im_x, im_y, stride);
	if ((flags & (RIGHT)) == 0 && pCurMV[1].all != MV_INTRA)
		pos[4] = get_pos(pCurMV[1], i + 1, j, im_x, im_y, stride);
	if ((flags & (BOTTOM | LEFT)) == 0 && pCurMV[-1+dimX].all != MV_INTRA)
		pos[5] = get_pos(pCurMV[-1+dimX], i - 1, j - 1, im_x, im_y, stride);
	if ((flags & (BOTTOM)) == 0 && pCurMV[+dimX].all != MV_INTRA)
		pos[6] = get_pos(pCurMV[+dimX], i, j - 1, im_x, im_y, stride);
	if ((flags & (BOTTOM | RIGHT)) == 0 && pCurMV[1+dimX].all != MV_INTRA)
		pos[7] = get_pos(pCurMV[1+dimX], i + 1, j - 1, im_x, im_y, stride);

	for( int c = 0; c < component; c++) {
		if ((flags & (TOP | LEFT)) == 0 && pCurMV[-1-dimX].all != MV_INTRA)
			list[0] = pRefFrames[pCurRef[-1-dimX]].pImage[c] + pos[0];
		if ((flags & (TOP)) == 0 && pCurMV[-dimX].all != MV_INTRA)
			list[1] = pRefFrames[pCurRef[-dimX]].pImage[c] + pos[1];
		if ((flags & (TOP | RIGHT)) == 0 && pCurMV[1-dimX].all != MV_INTRA)
			list[2] = pRefFrames[pCurRef[1-dimX]].pImage[c] + pos[2];
		if ((flags & (LEFT)) == 0 && pCurMV[-1].all != MV_INTRA)
			list[3] = pRefFrames[pCurRef[-1]].pImage[c] + pos[3];
		if ((flags & (RIGHT)) == 0 && pCurMV[1].all != MV_INTRA)
			list[4] = pRefFrames[pCurRef[1]].pImage[c] + pos[4];
		if ((flags & (BOTTOM | LEFT)) == 0 && pCurMV[-1+dimX].all != MV_INTRA)
			list[5] = pRefFrames[pCurRef[-1+dimX]].pImage[c] + pos[5];
		if ((flags & (BOTTOM)) == 0 && pCurMV[+dimX].all != MV_INTRA)
			list[6] = pRefFrames[pCurRef[+dimX]].pImage[c] + pos[6];
		if ((flags & (BOTTOM | RIGHT)) == 0 && pCurMV[1+dimX].all != MV_INTRA)
			list[7] = pRefFrames[pCurRef[1+dimX]].pImage[c] + pos[7];

		intra_block<flags>(list, dstImage.pImage[c] + (j * 8 - 4) * stride + i * 8 - 4, stride);
	}
}

template <bool pre, int pos_flags>
	void COBMC::intra_proc(int flags, short * pSrc, short * pDst, int stride)
{
	if (!(flags & (TOP | TOP_LEFT | LEFT) || pos_flags & (TOP | LEFT))) {
		CDCT2D::Proc_V<pre>(pSrc - stride * 4 - 4, stride);
		CDCT2D::Proc_V<pre>(pDst - stride * 4 - 4, stride);
	}
	if (!(flags & (TOP | TOP_RIGHT) || pos_flags & (TOP | RIGHT))) {
		CDCT2D::Proc_V<pre>(pSrc - stride * 4 + 4, stride);
		CDCT2D::Proc_V<pre>(pDst - stride * 4 + 4, stride);
	}
	if (!(flags & LEFT)) {
		if (!(pos_flags & (BOTTOM | LEFT))) {
			CDCT2D::Proc_V<pre>(pSrc + stride * 4 - 4, stride);
			CDCT2D::Proc_V<pre>(pDst + stride * 4 - 4, stride);
		}
		if (!(pos_flags & LEFT)) {
			CDCT2D::Proc_H<pre>(pSrc - 4, stride);
			CDCT2D::Proc_H<pre>(pDst - 4, stride);
		}
	}

	if (!(pos_flags & (BOTTOM | RIGHT))) {
		CDCT2D::Proc_V<pre>(pSrc + stride * 4 + 4, stride);
		CDCT2D::Proc_V<pre>(pDst + stride * 4 + 4, stride);
	}
	if (!(pos_flags & RIGHT)) {
		CDCT2D::Proc_H<pre>(pSrc + 4, stride);
		CDCT2D::Proc_H<pre>(pDst + 4, stride);
	}

	if (pre)
		copy<short,8,8,false>(pSrc, pDst, stride, stride);
}


#define OBMC(flags)	\
	{ \
		if (pCurMV[i].all != MV_INTRA) { \
			int src_pos = get_pos(pCurMV[i], i, j, im_x, im_y, stride); \
			for( int c = 0; c < component; c++) \
				obmc_block<flags>(pRefFrames[pCurRef[i]].pImage[c] + src_pos, \
					dstImage.pImage[c] + dst_pos, stride, stride); \
		} else \
			for( int c = 0; c < component; c++) \
				obmc_block_intra<flags>(dstImage.pImage[c] + dst_pos, stride, 0); \
	}

void COBMC::apply_mv(CImage * pRefFrames, CImage & dstImage)
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
				for( int c = 0; c < component; c++)
					obmc_block(pRefFrames[pCurRef[i]].pImage[c] + src_pos, dstImage.pImage[c] + dst_pos, stride, stride);
			} else
				for( int c = 0; c < component; c++)
					obmc_block_intra<0>(dstImage.pImage[c] + dst_pos, stride, 0);
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
}

template <int pos_flags>
int COBMC::get_intra_flags(sMotionVector * pCurMV, unsigned int dimX)
{
	int flags = 0;
	if (!(pos_flags & LEFT) && pCurMV[-1].all == MV_INTRA)
		flags |= LEFT;
	if (!(pos_flags & TOP) && pCurMV[-dimX].all == MV_INTRA)
		flags |= TOP;
	if (!(pos_flags & TOP | RIGHT) && pCurMV[-dimX+1].all == MV_INTRA)
		flags |= TOP_RIGHT;
	if (!(pos_flags & TOP | LEFT) && pCurMV[-dimX-1].all == MV_INTRA)
		flags |= TOP_LEFT;
	return flags;
}

template <bool pre>
void COBMC::apply_intra(CImage & srcImage, CImage & dstImage)
{
	const int stride = dstImage.dimXAlign;
	const int diff = dstImage.dimXAlign * 8 - dimX * 8 + 8;
	sMotionVector * pCurMV = pMV;
	int pos = 0, component = dstImage.component;
	unsigned int i = 0;

	if (pCurMV[i].all == MV_INTRA) {
		int flags = get_intra_flags<TOP | LEFT>(pCurMV + i, dimX);
		for( int c = 0; c < component; c++)
			intra_proc<pre,TOP | LEFT>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
	}
	pos += 8;
	for( i = 1; i < dimX - 1; i++){
		if (pCurMV[i].all == MV_INTRA) {
			int flags = get_intra_flags<TOP>(pCurMV + i, dimX);
			for( int c = 0; c < component; c++)
				intra_proc<pre,TOP>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
		}
		pos += 8;
	}
	if (pCurMV[i].all == MV_INTRA) {
		int flags = get_intra_flags<TOP | RIGHT>(pCurMV + i, dimX);
		for( int c = 0; c < component; c++)
			intra_proc<pre,TOP | RIGHT>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
	}
	pCurMV += dimX;
	pos += diff;
	for( unsigned int j = 2; j < dimY; j++) {
		i = 0;
		if (pCurMV[i].all == MV_INTRA) {
			int flags = get_intra_flags<LEFT>(pCurMV + i, dimX);
			for( int c = 0; c < component; c++)
				intra_proc<pre,LEFT>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
		}
		pos += 8;
		for( i = 1; i < dimX - 1; i++){
			if (pCurMV[i].all == MV_INTRA) {
				int flags = get_intra_flags<0>(pCurMV + i, dimX);
				for( int c = 0; c < component; c++)
					intra_proc<pre,0>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
			}
			pos += 8;
		}
		if (pCurMV[i].all == MV_INTRA) {
			int flags = get_intra_flags<RIGHT>(pCurMV + i, dimX);
			for( int c = 0; c < component; c++)
				intra_proc<pre,RIGHT>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
		}
		pCurMV += dimX;
		pos += diff;
	}

	i = 0;
	if (pCurMV[i].all == MV_INTRA) {
		int flags = get_intra_flags<BOTTOM | LEFT>(pCurMV + i, dimX);
		for( int c = 0; c < component; c++)
			intra_proc<pre,BOTTOM | LEFT>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
	}
	pos += 8;
	for( i = 1; i < dimX - 1; i++){
		if (pCurMV[i].all == MV_INTRA) {
			int flags = get_intra_flags<BOTTOM>(pCurMV + i, dimX);
			for( int c = 0; c < component; c++)
				intra_proc<pre,BOTTOM>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
		}
		pos += 8;
	}
	if (pCurMV[i].all == MV_INTRA) {
		int flags = get_intra_flags<BOTTOM | RIGHT>(pCurMV + i, dimX);
		for( int c = 0; c < component; c++)
			intra_proc<pre,BOTTOM | RIGHT>(flags, srcImage.pImage[c] + pos, dstImage.pImage[c] + pos, stride);
	}
}

template void COBMC::apply_intra<true>(CImage &, CImage &);
template void COBMC::apply_intra<false>(CImage &, CImage &);

#define MV_TABLE_SIZE 127u

const sHuffRL huff_mv_x_rl[] = {
	{2, 1}, {-1, 1}, {3, 1}, {1, 2}, {1, 2}, {1, 4}, {1, 4}, {1, 6}, {1, 8}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 5}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 2}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 9}, {1, 22}, {1, 1}, {-1, 3}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 1}, {-1, 1}, {1, 24}, {-4, 1}
};

const sHuffRL huff_mv_y_rl[] = {
	{1, 1}, {1, 1}, {2, 2}, {2, 3}, {1, 4}, {1, 3}, {-1, 1}, {2, 6}, {1, 5}, {1, 1}, {-1, 1}, {1, 6}, {1, 6}, {1, 1}, {-1, 1}, {1, 11}, {1, 1}, {-1, 2}, {1, 14}, {1, 5}, {1, 1}, {-1, 2}, {1, 4}, {-1, 1}, {1, 1}, {-1, 3}, {1, 29}, {-1, 1}, {1, 9}, {-6, 1}
};

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
					else
						MVPred = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1]);
				}
				if (pCurMV[i].x == MVPred.x && pCurMV[i].y == MVPred.y)
					zeroCodec.code0(0);
				else {
					zeroCodec.code1(0);
					int x = s2u(pCurMV[i].x - MVPred.x);
					int y = s2u(pCurMV[i].y - MVPred.y);
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
		}
		pCurMV += dimX;
	}
// 	huff.print(2);
// 	huff_x.print(2);
// 	huff_y.print(2);
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
					else
						MVPred = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1]);
				}
				if (zeroCodec.decode(0)) {
					int tmp = huff.decode(codec) + 1;
					int x = tmp & 0xF;
					int y = tmp >> 4;
					if (x == 15) {
						x += huff_x.decode(codec);
						if (x == 127 + 15)
							x += codec->golombLinDecode(5, 0);
					}
					pCurMV[i].x = u2s(x) + MVPred.x;
					if (y == 15) {
						y += huff_y.decode(codec);
						if (y == 127 + 15)
							y += codec->golombLinDecode(5, 0);
					}
					pCurMV[i].y = u2s(y) + MVPred.y;
				} else {
					pCurMV[i].x = MVPred.x;
					pCurMV[i].y = MVPred.y;
				}
			}
		}
		pCurMV += dimX;
	}
}

}
