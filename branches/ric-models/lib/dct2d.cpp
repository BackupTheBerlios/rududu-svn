/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti   *
 *   rududu@laposte.net   *
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
#include "dct2d.h"
#include "bindct.h"

namespace rududu {

CDCT2D::CDCT2D(int x, int y, int Align)
{
	DimX = x;
	DimY = y;
	DCTBand.Init(sshort, 64, x * y / 64, Align);
}

template <class C>
	void CDCT2D::DCT8_H(C * pBlock, int stride)
{
	C * x = pBlock;

	for( int j = 0; j < 8; j++){
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

template <class C>
	void CDCT2D::DCT8_V(C * pBlock, int stride)
{
	C * x[8];
	x[0] = pBlock;
	for( int i = 1; i < 8; i++)
		x[i] = x[i - 1] + stride;

	for( int j = 0; j < 8; j++){
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

template <class C>
	void CDCT2D::iDCT8_H(C * pBlock, int stride)
{
	C * x = pBlock;

	for( int j = 0; j < 8; j++){
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

template <class C>
	void CDCT2D::iDCT8_V(C * pBlock, int stride)
{
	C * x[8];
	x[0] = pBlock;
	for( int i = 1; i < 8; i++)
		x[i] = x[i - 1] + stride;

	for( int j = 0; j < 8; j++){
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

template <bool forward, class C>
void CDCT2D::Transform(C * pImage, int stride)
{
	C * pBand = (C*) DCTBand.pBand;
	for( int j = 0; j < DimY; j += 8){
		C * i = pImage;
		C * iend = i + DimX;
		pImage += stride * 8;
		for( ; i < iend; i += 8){
			int j = 0;
			if (forward) {
				for( C * k = i; k < pImage; k += stride){
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
				for( C * k = i; k < pImage; k += stride){
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

/*
template void CDCT2D::Transform<true>(short *, int);
template void CDCT2D::Transform<false>(short *, int);
*/

// pre / post filters from
// http://thanglong.ece.jhu.edu/Tran/Pub/prepost.pdf
template <bool pre, class C>
void CDCT2D::Proc_H(C * pBlock, int stride)
{
	C * x = pBlock;

	for( int j = 0; j < 8; j++){
		BFLY_FWD(x[0], x[7]);
		BFLY_FWD(x[1], x[6]);
		BFLY_FWD(x[2], x[5]);
		BFLY_FWD(x[3], x[4]);

		if (pre) {
			x[7] -= x[6] >> 1;
			x[6] += x[7] - (x[7] >> 2) - (x[5] >> 2);
			x[5] += x[6] >> 1;
			x[4] += x[5] >> 2;
		} else {
			x[4] -= x[5] >> 2;
			x[5] -= x[6] >> 1;
			x[6] -= x[7] - (x[7] >> 2) - (x[5] >> 2);
			x[7] += x[6] >> 1;
		}

		BFLY_INV(x[0], x[7]);
		BFLY_INV(x[1], x[6]);
		BFLY_INV(x[2], x[5]);
		BFLY_INV(x[3], x[4]);

		x += stride;
	}
}

template <bool pre, class C>
void CDCT2D::Proc_V(C * pBlock, int stride)
{
	C * x[8];
	x[0] = pBlock;
	for( int i = 1; i < 8; i++)
		x[i] = x[i - 1] + stride;

	for( int j = 0; j < 8; j++){
		BFLY_FWD(x[0][j], x[7][j]);
		BFLY_FWD(x[1][j], x[6][j]);
		BFLY_FWD(x[2][j], x[5][j]);
		BFLY_FWD(x[3][j], x[4][j]);

		if (pre) {
			x[7][j] -= x[6][j] >> 1;
			x[6][j] += x[7][j] - (x[7][j] >> 2) - (x[5][j] >> 2);
			x[5][j] += x[6][j] >> 1;
			x[4][j] += x[5][j] >> 2;
		} else {
			x[4][j] -= x[5][j] >> 2;
			x[5][j] -= x[6][j] >> 1;
			x[6][j] -= x[7][j] - (x[7][j] >> 2) - (x[5][j] >> 2);
			x[7][j] += x[6][j] >> 1;
		}

		BFLY_INV(x[0][j], x[7][j]);
		BFLY_INV(x[1][j], x[6][j]);
		BFLY_INV(x[2][j], x[5][j]);
		BFLY_INV(x[3][j], x[4][j]);
	}
}

template <bool pre, class C>
void CDCT2D::Proc(C * pImage, int stride)
{
	C * i, * iend;

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

/*
template void CDCT2D::Proc<true>(short *, int);
template void CDCT2D::Proc<false>(short *, int);
*/

const float CDCT2D::norm[8] = {.353553391f, .707106781, .461939766f, .5411961f, .707106781, .5f, .5f, .353553391f};

template <class C>
	unsigned int CDCT2D::TSUQ(C Quant, float Thres)
{
	int iQuant[64], Count = 0;
	C T[64];
	C * pBand = (C*) DCTBand.pBand;

	Quant = (Quant + 1) >> 1;

	for( int j = 0; j < 8; j++){
		for( int i = 0; i < 8; i++){
			iQuant[j * 8 + i] = (((int)(Quant / (norm[i] * norm[j]))) + 8) & (-1 << 4);
			T[j * 8 + i] = (C) (Thres * iQuant[j * 8 + i]);
			iQuant[j * 8 + i] = (1 << 16) / iQuant[j * 8 + i];
		}
	}

	for ( unsigned int j = 0; j < DCTBand.DimY ; j++ ) {
		for ( int i = 0; i < 64 ; i++ ) {
			if ( U(pBand[i] + T[i]) <= U(2 * T[i])) {
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

template <class C>
	void CDCT2D::TSUQi(C Quant)
{
	int Q[64];
	C * pBand = (C*) DCTBand.pBand;

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
