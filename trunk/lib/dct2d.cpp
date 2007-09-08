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
#include "dct2d.h"

namespace rududu {

CDCT2D::CDCT2D(int x, int y, int Align)
{
	DimX = x;
	DimY = y;
	DCTBand.Init(64, x * y / 64, Align);
}


CDCT2D::~CDCT2D()
{
}

#define BFLY(a,b)	tmp = a; a += b; b = tmp - b;

#define P1(a)	((a >> 1) - (a >> 4))	// 7/16
#define U1(a)	((a >> 1) - (a >> 3))	// 3/8
#define P2(a)	(a >> 2)	// 1/4
#define U2(a)	((a >> 1) + (a >> 4))	// 9/16
#define P3(a)	((a >> 2) + (a >> 4))	// 5/16
#define P4(a)	(a >> 3)	// 1/8
#define U3(a)	((a >> 2) - (a >> 4))	// 3/16
#define P5(a)	((a >> 3) - (a >> 5))	// 3/32

// #define P1(a)	((a >> 1) - (a >> 3))	// 3/8
// #define U1(a)	(a >> 2)	// 1/4
// #define P2(a)	(a >> 2)	// 1/4
// #define U2(a)	(a >> 1)	// 1/2
// #define P3(a)	(a >> 2)	// 1/4
// #define P4(a)	(a >> 3)	// 1/8
// #define U3(a)	((a >> 2) - (a >> 4))	// 3/16
// #define P5(a)	((a >> 3) - (a >> 5))	// 3/32

// #define P1(a)	(a >> 1)	// 1/2
// #define U1(a)	(a >> 1)	// 1/2
// #define P2(a)	(a >> 2)	// 1/4
// #define U2(a)	(a >> 1)	// 1/2
// #define P3(a)	(a >> 2)	// 1/4
// #define P4(a)	(a >> 3)	// 1/8
// #define U3(a)	(a >> 2)	// 1/4
// #define P5(a)	(a >> 3)	// 1/8

// #define P1(a)	(a >> 1)	// 1/2
// #define U1(a)	(a >> 1)	// 1/2
// #define P2(a)	0
// #define U2(a)	(a >> 1)	// 1/2
// #define P3(a)	(a >> 2)	// 1/4
// #define P4(a)	0
// #define U3(a)	(a >> 2)	// 1/4
// #define P5(a)	0

void CDCT2D::DCT8_H(short * pBlock, int stride)
{
	short * x = pBlock;

	for( int j = 0; j < 8; j++){
		short tmp;

		BFLY(x[0], x[7]);
		BFLY(x[1], x[6]);
		BFLY(x[2], x[5]);
		BFLY(x[3], x[4]);

		BFLY(x[0], x[3]);
		BFLY(x[1], x[2]);

		x[0] += x[1];
		x[1] -= x[0] >> 1;

		x[2] -= P1(x[3]);
		x[3] -= U1(x[2]);

		x[7] -= P2(x[4]);
		x[4] += U2(x[7]);
		x[7] -= P3(x[4]);

		x[6] -= P4(x[5]);
		x[5] += U3(x[6]);
		x[6] -= P5(x[5]);

		BFLY(x[4], x[6]);
		BFLY(x[7], x[5]);

		x[7] += x[4];
		x[4] -= x[7] >> 1;

		x += stride;
	}
}

void CDCT2D::DCT8_V(short * pBlock, int stride)
{
	short * x[8];
	x[0] = pBlock;
	for( int i = 1; i < 8; i++)
		x[i] = x[i - 1] + stride;

	for( int j = 0; j < 8; j++){
		short tmp;

		BFLY(x[0][j], x[7][j]);
		BFLY(x[1][j], x[6][j]);
		BFLY(x[2][j], x[5][j]);
		BFLY(x[3][j], x[4][j]);

		BFLY(x[0][j], x[3][j]);
		BFLY(x[1][j], x[2][j]);

		x[0][j] += x[1][j];
		x[1][j] -= x[0][j] >> 1;

		x[2][j] -= P1(x[3][j]);
		x[3][j] -= U1(x[2][j]);

		x[7][j] -= P2(x[4][j]);
		x[4][j] += U2(x[7][j]);
		x[7][j] -= P3(x[4][j]);

		x[6][j] -= P4(x[5][j]);
		x[5][j] += U3(x[6][j]);
		x[6][j] -= P5(x[5][j]);

		BFLY(x[4][j], x[6][j]);
		BFLY(x[7][j], x[5][j]);

		x[7][j] += x[4][j];
		x[4][j] -= x[7][j] >> 1;
	}
}

void CDCT2D::iDCT8_H(short * pBlock, int stride)
{
	short * x = pBlock;

	for( int j = 0; j < 8; j++){
		short tmp;

		x[4] += x[7] >> 1;
		x[7] -= x[4];

		BFLY(x[4], x[6]);
		BFLY(x[7], x[5]);

		x[6] += P5(x[5]);
		x[5] -= U3(x[6]);
		x[6] += P4(x[5]);

		x[7] += P3(x[4]);
		x[4] -= U2(x[7]);
		x[7] += P2(x[4]);

		x[3] += U1(x[2]);
		x[2] += P1(x[3]);

		x[1] += x[0] >> 1;
		x[0] -= x[1];

		BFLY(x[0], x[3]);
		BFLY(x[1], x[2]);

		BFLY(x[0], x[7]);
		BFLY(x[1], x[6]);
		BFLY(x[2], x[5]);
		BFLY(x[3], x[4]);

		x += stride;
	}
}

void CDCT2D::iDCT8_V(short * pBlock, int stride)
{
	short * x[8];
	x[0] = pBlock;
	for( int i = 1; i < 8; i++)
		x[i] = x[i - 1] + stride;

	for( int j = 0; j < 8; j++){
		short tmp;

		x[4][j] += x[7][j] >> 1;
		x[7][j] -= x[4][j];

		BFLY(x[4][j], x[6][j]);
		BFLY(x[7][j], x[5][j]);

		x[6][j] += P5(x[5][j]);
		x[5][j] -= U3(x[6][j]);
		x[6][j] += P4(x[5][j]);

		x[7][j] += P3(x[4][j]);
		x[4][j] -= U2(x[7][j]);
		x[7][j] += P2(x[4][j]);

		x[3][j] += U1(x[2][j]);
		x[2][j] += P1(x[3][j]);

		x[1][j] += x[0][j] >> 1;
		x[0][j] -= x[1][j];

		BFLY(x[0][j], x[3][j]);
		BFLY(x[1][j], x[2][j]);

		BFLY(x[0][j], x[7][j]);
		BFLY(x[1][j], x[6][j]);
		BFLY(x[2][j], x[5][j]);
		BFLY(x[3][j], x[4][j]);
	}
}

template <bool forward>
void CDCT2D::Transform(short * pImage, int stride)
{
	short * pBand = DCTBand.pBand;
	for( int j = 0; j < DimY; j += 8){
		short * i = pImage;
		short * iend = i + DimX;
		pImage += stride * 8;
		for( ; i < iend; i += 8){
			int j = 0;
			if (forward) {
				for( short * k = i; k < pImage; k += stride){
					do {
						pBand[j] = k[j & 7];
						j++;
					} while ((j & 7) != 0);
				}
				DCT8_V(pBand, 8);
				DCT8_H(pBand, 8);
			} else {
				iDCT8_H(pBand, 8);
				iDCT8_V(pBand, 8);
				for( short * k = i; k < pImage; k += stride){
					do {
						k[j & 7] = pBand[j];
						j++;
					} while ((j & 7) != 0);
				}
			}
			pBand += DCTBand.DimXAlign;
		}
	}
}

template void CDCT2D::Transform<true>(short *, int);
template void CDCT2D::Transform<false>(short *, int);

template <bool pre>
void CDCT2D::Proc_H(short * pBlock, int stride)
{
	short * x = pBlock;

	for( int j = 0; j < 8; j++){
		short tmp;

		BFLY(x[0], x[7]);
		BFLY(x[1], x[6]);
		BFLY(x[2], x[5]);
		BFLY(x[3], x[4]);

		if (pre) {
			x[4] *= 8;
			x[5] = (x[5] * 43691) >> 14;
			x[6] = (x[6] * 26214) >> 14;
			x[7] = (x[7] * 18725) >> 14;
		} else {
			x[4] >>= 3;
			x[5] = (x[5] >> 1) - (x[5] >> 3);
			x[6] = (x[6] >> 1) + (x[6] >> 3);
			x[7] -= x[7] >> 3;
		}

		BFLY(x[0], x[7]);
		BFLY(x[1], x[6]);
		BFLY(x[2], x[5]);
		BFLY(x[3], x[4]);

		for( int i = 0; i < 8; i++) x[i] >>= 1;

		x += stride;
	}
}

template <bool pre>
void CDCT2D::Proc_V(short * pBlock, int stride)
{
	short * x[8];
	x[0] = pBlock;
	for( int i = 1; i < 8; i++)
		x[i] = x[i - 1] + stride;

	for( int j = 0; j < 8; j++){
		short tmp;

		BFLY(x[0][j], x[7][j]);
		BFLY(x[1][j], x[6][j]);
		BFLY(x[2][j], x[5][j]);
		BFLY(x[3][j], x[4][j]);

		if (pre) {
			x[4][j] *= 8;
			x[5][j] = (x[5][j] * 43691) >> 14;
			x[6][j] = (x[6][j] * 26214) >> 14;
			x[7][j] = (x[7][j] * 18725) >> 14;
		} else {
			x[4][j] >>= 3;
			x[5][j] = (x[5][j] >> 1) - (x[5][j] >> 3);
			x[6][j] = (x[6][j] >> 1) + (x[6][j] >> 3);
			x[7][j] -= x[7][j] >> 3;
		}

		BFLY(x[0][j], x[7][j]);
		BFLY(x[1][j], x[6][j]);
		BFLY(x[2][j], x[5][j]);
		BFLY(x[3][j], x[4][j]);

		for( int i = 0; i < 8; i++) x[i][j] >>= 1;
	}
}

template <bool pre>
void CDCT2D::Proc(short * pImage, int stride)
{
	short * i, * iend;

	for( int j = 8; j < DimY; j += 8){
		i = pImage + 4 * stride;
		iend = i + DimX;
		for( ; i < iend; i += 8){
			Proc_V<pre>(i, stride);
		}
		i = pImage + 4;
		iend = i + DimX - 8;
		for( ; i < iend; i += 8){
			Proc_H<pre>(i, stride);
		}
		pImage += stride * 8;
	}

	i = pImage + 4;
	iend = i + DimX - 8;
	for( ; i < iend; i += 8){
		Proc_H<pre>(i, stride);
	}
}

template void CDCT2D::Proc<true>(short *, int);
template void CDCT2D::Proc<false>(short *, int);

const float CDCT2D::norm[8] = {.353553391f, .707106781, .461939766f, .5411961f, .707106781, .5f, .5f, .353553391f};

unsigned int CDCT2D::TSUQ(short Quant, float Thres)
{
	int iQuant[64], Count = 0;
	short T[64];
	short * pBand = DCTBand.pBand;

	Quant = (Quant + 1) >> 1;

	for( int j = 0; j < 8; j++){
		for( int i = 0; i < 8; i++){
			iQuant[j * 8 + i] = (((int)(Quant / (norm[i] * norm[j]))) + 8) & (-1 << 4);
			T[j * 8 + i] = (short) (Thres * iQuant[j * 8 + i]);
			iQuant[j * 8 + i] = (1 << 16) / iQuant[j * 8 + i];
		}
	}

	for ( unsigned int j = 0; j < DCTBand.DimY ; j++ ) {
		for ( int i = 0; i < 64 ; i++ ) {
			if ( (unsigned short) (pBand[i] + T[i]) <= (unsigned short) (2 * T[i])) {
				pBand[i] = 0;
			} else {
				Count++;
				pBand[i] = (pBand[i] * iQuant[i] + (1 << 15)) >> 16;
			}
		}
		pBand += DCTBand.DimXAlign;
	}

	DCTBand.Count = Count;
	return Count;
}

void CDCT2D::TSUQi(short Quant)
{
	int Q[64];
	short * pBand = DCTBand.pBand;

	Quant = (Quant + 1) >> 1;

	for( int j = 0; j < 8; j++){
		for( int i = 0; i < 8; i++){
			Q[j * 8 + i] = (((int)(Quant / (norm[i] * norm[j]))) + 8) >> 4;
		}
	}

	for ( unsigned int j = 0; j < DCTBand.DimY ; j++ ) {
		for ( int i = 0; i < 64 ; i++ ) {
			pBand[i] = pBand[i] * Q[i];
		}
		pBand += DCTBand.DimXAlign;
	}
}

}
