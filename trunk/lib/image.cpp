/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti                                   *
 *   rududu@laposte.net                                                    *
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

#include "image.h"
#include "utils.h"

using namespace std;

namespace rududu {

CImage::CImage(unsigned int x, unsigned int y, int cmpnt, int Align):
		pData(0)
{
	Init(x, y, cmpnt, Align);
}

CImage::~CImage()
{
	delete[] pData;
}

void CImage::Init(unsigned int x, unsigned int y, int cmpnt, int Align)
{
	dimX = x;
	dimY = y;
	component = cmpnt;
	dimXAlign = ( dimX + 2 * BORDER + Align - 1 ) & ( -Align );
	if ( (dimXAlign * dimY) != 0){
		pData = new char[(dimXAlign * (dimY + 2 * BORDER)) * sizeof(short) * component + Align];
		pImage[0] = (short*)(((intptr_t)pData + Align - 1) & (-Align));
		pImage[0] += BORDER * dimXAlign + BORDER;
		for( int i = 1; i < component; i++){
			pImage[i] = pImage[i - 1] + (dimXAlign * (dimY + 2 * BORDER));
		}
	}
}

template <class input_t>
void CImage::inputRGB(input_t * pIn, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( int j = 0; j < dimY; j++){
		for (int i = 0, k = 0; i < dimX; i++ , k += 3){
			Co[i] = pIn[k] - pIn[k + 2];
			Y[i] = pIn[k + 2] + (Co[i] >> 1);
			Cg[i] = pIn[k + 1] - Y[i];
			Y[i] += (Cg[i] >> 1) + offset;
			if (sizeof(input_t) == 1) {
				Y[i] <<= 4;
				Co[i] <<= 3;
				Cg[i] <<= 3;
			}
		}
		Y += dimXAlign;
		Co += dimXAlign;
		Cg += dimXAlign;
		pIn += stride * 3;
	}
}

template void CImage::inputRGB<char>(char*, int, short);

template <class input_t>
void CImage::inputSGI(input_t * pIn, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];
	input_t * R = pIn + stride * dimY;
	input_t * G = R + stride * dimY;
	input_t * B = G + stride * dimY;

	for( int j = 0; j < dimY; j++){
		R -= stride;
		G -= stride;
		B -= stride;
		for (int i = 0; i < dimX; i++){
			Co[i] = R[i] - B[i];
			Y[i] = B[i] + (Co[i] >> 1);
			Cg[i] = G[i] - Y[i];
			Y[i] += (Cg[i] >> 1) + offset;
			if (sizeof(input_t) == 1) {
				Y[i] <<= 4;
				Co[i] <<= 3;
				Cg[i] <<= 3;
			}
		}
		Y += dimXAlign;
		Co += dimXAlign;
		Cg += dimXAlign;
	}
}

template void CImage::inputSGI<unsigned char>(unsigned char*, int, short);

template <class output_t>
void CImage::outputRGB(output_t * pOut, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( int j = 0; j < dimY; j++){
		for (int i = 0, k = 0; i < dimX; i++ , k += 3){
			pOut[k + 2] = Y[i] - (Cg[i] >> 1) - offset;
			pOut[k + 1] = Cg[i] + pOut[k + 2];
			pOut[k + 2] -= Co[i] >> 1;
			pOut[k] = Co[i] + pOut[k + 2];
		}
		Y += dimXAlign;
		Co += dimXAlign;
		Cg += dimXAlign;
		pOut += stride;
	}
}

template void CImage::outputRGB<char>(char*, int, short);

template <class output_t, bool i420>
void CImage::outputYV12(output_t * pOut, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];
	output_t * Yo = pOut;
	output_t * Vo = Yo + stride * dimY;
	output_t * Uo = Vo + ((stride * dimY) >> 2);

	const int shift = 12 - sizeof(output_t) * 8;
	if (sizeof(output_t) == 1) offset <<= 4;
	else if (sizeof(output_t) == 2) offset >>= 4;

	if (i420) {
		output_t * tmp = Uo;
		Uo = Vo;
		Vo = tmp;
	}

	for( int j = 0; j < dimY; j += 2){
		for (int i = 0; i < dimX; i += 2){
			Yo[i] = ((440 * (Y[i] - offset) + 82 * Co[i] + 76 * Cg[i] + (1 << (8 + shift))) >> (9 + shift)) + 16;
			Yo[i+1] = ((440 * (Y[i+1] - offset) + 82 * Co[i+1] + 76 * Cg[i+1] + (1 << (8 + shift))) >> (9 + shift)) + 16;
			Yo[i+stride] = ((440 * (Y[i+dimXAlign] - offset) + 82 * Co[i+dimXAlign] + 76 * Cg[i+dimXAlign] + (1 << (8 + shift))) >> (9 + shift)) + 16;
			Yo[i+stride+1] = ((440 * (Y[i+dimXAlign+1] - offset) + 82 * Co[i+dimXAlign+1] + 76 * Cg[i+dimXAlign+1] + (1 << (8 + shift))) >> (9 + shift)) + 16;

			Uo[i>>1] = ((- 150 * (Co[i] + Co[i+1] + Co[i+dimXAlign] + Co[i+dimXAlign+1])
			             - 148 * (Cg[i] + Cg[i+1] + Cg[i+dimXAlign] + Cg[i+dimXAlign+1]) + (1 << (9 + shift))) >> (10 + shift)) + 128;
			Vo[i>>1] = ((130 * (Co[i] + Co[i+1] + Co[i+dimXAlign] + Co[i+dimXAlign+1])
			             - 188 * (Cg[i] + Cg[i+1] + Cg[i+dimXAlign] + Cg[i+dimXAlign+1]) + (1 << (9 + shift))) >> (10 + shift)) + 128;
		}
		Y += dimXAlign * 2;
		Co += dimXAlign * 2;
		Cg += dimXAlign * 2;
		Yo += stride * 2;
		Vo += stride >> 1;
		Uo += stride >> 1;
	}
}

template void CImage::outputYV12<char, false>(char*, int, short);
template void CImage::outputYV12<char, true>(char*, int, short);

void CImage::extend(void)
{
	for( int j = 0; j < component; j++) {
		for( short * s1 = pImage[j] - BORDER, * s2 = s1 + dimX, * end = pImage[j] + dimXAlign * dimY; s2 < end; ) {
			for( int i = 0; i < BORDER; i++) {
				s1[i] = s1[BORDER];
				s2[i] = s2[-1];
			}
		}

		short * dest = pImage[j] - BORDER;
		for( int i = 0; i < BORDER; i++) {
			memcpy(dest - dimXAlign, dest, (dimX + 2 * BORDER) * sizeof(short));
			dest -= dimXAlign;
		}

		dest = pImage[j] - BORDER + dimXAlign * dimY;
		for( int i = 0; i < BORDER; i++) {
			memcpy(dest + dimXAlign, dest, (dimX + 2 * BORDER) * sizeof(short));
			dest += dimXAlign;
		}
	}
}

CImage & CImage::operator-= (const CImage & pIn)
{
	for (int c = 0; c < component; c++) {
		short * out = pImage[c];
		short * in = pIn.pImage[c];
		for (int j = 0; j < dimY; j++) {
			for (int i = 0; i < dimX; i++) {
				out[i] -= in[i];
			}
			out += dimXAlign;
			in += pIn.dimXAlign;
		}
	}
	return *this;
}

}
