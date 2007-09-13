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

#define MV_TABLE_SIZE 127

const sHuffSym huff_mv_x_dec1[] = {
	{0x8000, 1, 1}, {0x4000, 2, 2}, {0x3000, 4, 5}, {0x2000, 5, 8}, {0x1800, 6, 12}, {0x1000, 7, 18}, {0xc00, 8, 26}, {0x900, 9, 38}, {0x480, 10, 56}, {0x220, 11, 74}, {0x150, 12, 91}, {0x70, 13, 112}, {0x0, 14, 126}
}; // 13
const unsigned char huff_mv_x_dec2[MV_TABLE_SIZE] = { 2, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 22, 25, 24, 27, 47, 49, 29, 33, 31, 45, 127, 26, 35, 51, 37, 39, 28, 41, 43, 53, 55, 30, 57, 59, 32, 34, 36, 38, 50, 61, 40, 42, 44, 48, 46, 52, 56, 63, 54, 58, 60, 62, 64, 65, 66, 67, 68, 69, 70, 71, 73, 79, 74, 75, 76, 77, 88, 90, 92, 94, 72, 78, 80, 86, 82, 84, 85, 96, 81, 83, 91, 87, 89, 98, 100, 95, 102, 93, 97, 104, 106, 107, 108, 115, 121, 99, 101, 103, 110, 113, 114, 117, 118, 119, 120, 122, 124, 105, 109, 111, 112, 116, 123, 125, 126 };

const sHuffSym huff_mv_x_enc[MV_TABLE_SIZE] = {
	{1, 2}, {1, 1}, {3, 4}, {5, 5}, {4, 5}, {7, 6}, {6, 6}, {11, 7}, {10, 7}, {9, 7}, {8, 7}, {15, 8}, {14, 8}, {13, 8}, {12, 8}, {23, 9}, {22, 9}, {21, 9}, {20, 9}, {19, 9}, {18, 9}, {34, 10}, {35, 10}, {32, 10}, {33, 10}, {23, 10}, {31, 10}, {18, 10}, {28, 10}, {31, 11}, {26, 10}, {28, 11}, {27, 10}, {27, 11}, {22, 10}, {26, 11}, {20, 10}, {25, 11}, {19, 10}, {22, 11}, {35, 11}, {21, 11}, {34, 11}, {20, 11}, {25, 10}, {18, 11}, {30, 10}, {19, 11}, {29, 10}, {24, 11}, {21, 10}, {17, 11}, {33, 11}, {31, 12}, {32, 11}, {33, 12}, {30, 11}, {30, 12}, {29, 11}, {29, 12}, {23, 11}, {28, 12}, {32, 12}, {27, 12}, {26, 12}, {25, 12}, {24, 12}, {23, 12}, {22, 12}, {21, 12}, {41, 13}, {30, 13}, {40, 13}, {38, 13}, {37, 13}, {36, 13}, {35, 13}, {29, 13}, {39, 13}, {28, 13}, {22, 13}, {26, 13}, {21, 13}, {25, 13}, {24, 13}, {27, 13}, {19, 13}, {34, 13}, {18, 13}, {33, 13}, {20, 13}, {32, 13}, {27, 14}, {31, 13}, {15, 13}, {23, 13}, {26, 14}, {17, 13}, {19, 14}, {16, 13}, {18, 14}, {14, 13}, {17, 14}, {25, 14}, {7, 14}, {24, 14}, {23, 14}, {22, 14}, {6, 14}, {16, 14}, {5, 14}, {4, 14}, {15, 14}, {14, 14}, {21, 14}, {3, 14}, {13, 14}, {12, 14}, {11, 14}, {10, 14}, {20, 14}, {9, 14}, {2, 14}, {8, 14}, {1, 14}, {0, 14}, {24, 10}
};

const sHuffSym huff_mv_y_dec1[] = {
	{0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 4, 5}, {0x1400, 6, 11}, {0xa00, 7, 16}, {0x700, 8, 21}, {0x400, 9, 28}, {0x240, 10, 36}, {0x160, 11, 45}, {0xf0, 12, 56}, {0x80, 13, 71}, {0x44, 14, 87}, {0x2c, 15, 104}, {0x0, 16, 126}
}; // 14

const unsigned char huff_mv_y_dec2[MV_TABLE_SIZE] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 11, 13, 12, 14, 17, 16, 18, 19, 20, 21, 22, 23, 24, 127, 25, 26, 28, 27, 30, 29, 31, 32, 34, 33, 35, 36, 37, 39, 38, 40, 42, 41, 44, 43, 48, 45, 46, 47, 49, 51, 52, 50, 53, 55, 56, 54, 57, 58, 60, 65, 59, 61, 62, 63, 66, 68, 64, 67, 69, 70, 72, 71, 73, 77, 78, 74, 75, 83, 85, 86, 87, 117, 76, 79, 80, 81, 82, 84, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 118, 119, 120, 121, 122, 123, 124, 125, 126 };

const sHuffSym huff_mv_y_enc[MV_TABLE_SIZE] = {
	{1, 1}, {1, 2}, {3, 4}, {2, 4}, {7, 6}, {6, 6}, {5, 6}, {9, 7}, {8, 7}, {7, 7}, {5, 7}, {8, 8}, {9, 8}, {7, 8}, {6, 7}, {12, 9}, {13, 9}, {11, 9}, {10, 9}, {9, 9}, {8, 9}, {15, 10}, {14, 10}, {13, 10}, {11, 10}, {10, 10}, {17, 11}, {9, 10}, {15, 11}, {16, 11}, {14, 11}, {13, 11}, {11, 11}, {12, 11}, {21, 12}, {20, 12}, {19, 12}, {17, 12}, {18, 12}, {16, 12}, {29, 13}, {15, 12}, {27, 13}, {28, 13}, {25, 13}, {24, 13}, {23, 13}, {26, 13}, {22, 13}, {19, 13}, {21, 13}, {20, 13}, {18, 13}, {31, 14}, {17, 13}, {16, 13}, {30, 14}, {29, 14}, {26, 14}, {28, 14}, {25, 14}, {24, 14}, {23, 14}, {20, 14}, {27, 14}, {22, 14}, {19, 14}, {21, 14}, {18, 14}, {17, 14}, {32, 15}, {33, 15}, {31, 15}, {28, 15}, {27, 15}, {43, 16}, {30, 15}, {29, 15}, {42, 16}, {41, 16}, {40, 16}, {39, 16}, {26, 15}, {38, 16}, {25, 15}, {24, 15}, {23, 15}, {37, 16}, {36, 16}, {35, 16}, {34, 16}, {33, 16}, {32, 16}, {31, 16}, {30, 16}, {29, 16}, {28, 16}, {27, 16}, {26, 16}, {25, 16}, {24, 16}, {23, 16}, {22, 16}, {21, 16}, {20, 16}, {19, 16}, {18, 16}, {17, 16}, {16, 16}, {15, 16}, {14, 16}, {13, 16}, {12, 16}, {11, 16}, {10, 16}, {9, 16}, {22, 15}, {8, 16}, {7, 16}, {6, 16}, {5, 16}, {4, 16}, {3, 16}, {2, 16}, {1, 16}, {0, 16}, {12, 10}
};

void COBMC::encode(CMuxCodec * outCodec)
{
	sMotionVector * pCurMV = pMV;
	CBitCodec intraCodec(outCodec), zeroCodec(outCodec);

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
				if (pCurMV[i].x == MVPred.x)
					zeroCodec.code0(0);
				else {
					zeroCodec.code1(0);
					int tmp = s2u(pCurMV[i].x - MVPred.x) - 1;
					if (tmp >= MV_TABLE_SIZE - 1) {
						outCodec->bitsCode(huff_mv_x_enc[MV_TABLE_SIZE - 1].code, huff_mv_x_enc[MV_TABLE_SIZE - 1].len);
						outCodec->golombLinCode(tmp - MV_TABLE_SIZE + 1, 5, 0);
					} else {
						outCodec->bitsCode(huff_mv_x_enc[tmp].code, huff_mv_x_enc[tmp].len);
					}
				}

				if (pCurMV[i].y == MVPred.y)
					zeroCodec.code0(1);
				else {
					zeroCodec.code1(1);
					int tmp = s2u(pCurMV[i].y - MVPred.y) - 1;
					if (tmp >= MV_TABLE_SIZE - 1) {
						outCodec->bitsCode(huff_mv_y_enc[MV_TABLE_SIZE - 1].code, huff_mv_y_enc[MV_TABLE_SIZE - 1].len);
						outCodec->golombLinCode(tmp - MV_TABLE_SIZE + 1, 7, 0);
					} else {
						outCodec->bitsCode(huff_mv_y_enc[tmp].code, huff_mv_y_enc[tmp].len);
					}
				}
			}
		}
		pCurMV += dimX;
	}
}

void COBMC::decode(CMuxCodec * inCodec)
{
	sMotionVector * pCurMV = pMV;
	CBitCodec intraCodec(inCodec), zeroCodec(inCodec);

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
					int tmp = huff_mv_x_dec2[inCodec->huffDecode(huff_mv_x_dec1)];
					if (tmp == MV_TABLE_SIZE - 1)
						tmp = inCodec->golombLinDecode(5, 0) + MV_TABLE_SIZE - 1;
					pCurMV[i].x = u2s(tmp + 1) + MVPred.x;
				} else
					pCurMV[i].x = 0;

				if (zeroCodec.decode(1)) {
					int tmp = huff_mv_y_dec2[inCodec->huffDecode(huff_mv_y_dec1)];
					if (tmp == MV_TABLE_SIZE - 1)
						tmp = inCodec->golombLinDecode(7, 0) + MV_TABLE_SIZE - 1;
					pCurMV[i].y = u2s(tmp + 1) + MVPred.y;
				} else
					pCurMV[i].y = 0;
			}
		}
		pCurMV += dimX;
	}
}

}
