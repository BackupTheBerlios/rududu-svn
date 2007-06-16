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

namespace rududu {

template <int component>
CImage<component>::CImage(unsigned int x, unsigned int y, int Align):
		pData(0)
{
	Init(x, y, Align);
}

template <int component>
CImage<component>::~CImage()
{
	delete[] pData;
}

template <int component>
void CImage<component>::Init(unsigned int x, unsigned int y, int Align)
{
	DimX = x;
	DimY = y;
	DimXAlign = ( DimX + 2 * BORDER + Align - 1 ) & ( -Align );
	if ( (DimXAlign * DimY) != 0){
		pData = new char[(DimXAlign * (DimY + 2 * BORDER)) * sizeof(short) * component + Align];
		pImage[0] = (short*)(((intptr_t)pData + Align - 1) & (-Align));
		pImage[0] += BORDER * DimXAlign + BORDER;
		for( int i = 1; i < component; i++){
			pImage[i] = pImage[i - 1] + (DimXAlign * (DimY + 2 * BORDER));
		}
	}
}

template <int component> template <class input_t>
void CImage<component>::inputRGB(input_t * pIn, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( int j = 0; j < DimY; j++){
		for (int i = 0, k = 0; i < DimX; i++ , k += 3){
			Co[i] = pIn[k] - pIn[k + 2];
			Y[i] = pIn[k + 2] + (Co[i] >> 1);
			Cg[i] = pIn[k + 1] - Y[i];
			Y[i] += (Cg[i] >> 1) + offset;
		}
		Y += DimXAlign;
		Co += DimXAlign;
		Cg += DimXAlign;
		pIn += stride;
	}
}

template <int component> template <class output_t>
void CImage<component>::outputRGB(output_t * pOut, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( int j = 0; j < DimY; j++){
		for (int i = 0, k = 0; i < DimX; i++ , k += 3){
			pOut[k + 2] = Y[i] - (Cg[i] >> 1) - offset;
			pOut[k + 1] = Cg[i] + pOut[k + 2];
			pOut[k + 2] -= Co[i] >> 1;
			pOut[k] = Co[i] + pOut[k + 2];
		}
		Y += DimXAlign;
		Co += DimXAlign;
		Cg += DimXAlign;
		pOut += stride;
	}
}


/**
 * Y   .7065  .0925  .087    Y
 * U = 1.4785 -.2935 -.299 * Co
 * V   .6195  .0925  0       Cg
 *
 * (without rescale and offset)
 *
 * @param pOut
 * @param stride
 * @param offset
 */
template <int component> template <class output_t>
void CImage<component>::outputYUV(output_t * pOut, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( int j = 0; j < DimY; j++){
		for (int i = 0, k = 0; i < DimX; i++ , k += 3){
			pOut[k] = (723 * Y[i] + 95 * Co[i] + 89 * Cg[i]) >> 10;
			pOut[k + 1] = (1514 * Y[i] - 301 * Co[i] - 306 * Cg[i]) >> 10;
			pOut[k + 2] = (634 * Y[i] + 95 * Co[i]) >> 10;
		}
		Y += DimXAlign;
		Co += DimXAlign;
		Cg += DimXAlign;
		pOut += stride;
	}
}

template <int component>
void CImage<component>::extend(void)
{
	for( int j = 0; j < component; j++) {
		for( short * s1 = pImage[j] - BORDER, * s2 = s1 + DimX, * end = pImage[j] + DimXAlign * DimY; s2 < end; ) {
			for( int i = 0; i < BORDER; i++) {
				s1[i] = s1[BORDER];
				s2[i] = s2[-1];
			}
		}

		short * dest = pImage[j] - BORDER;
		for( int i = 0; i < BORDER; i++) {
			memcpy(dest - DimXAlign, dest, (DimX + 2 * BORDER) * sizeof(short));
			dest -= DimXAlign;
		}

		dest = pImage[j] - BORDER + DimXAlign * DimY;
		for( int i = 0; i < BORDER; i++) {
			memcpy(dest + DimXAlign, dest, (DimX + 2 * BORDER) * sizeof(short));
			dest += DimXAlign;
		}
	}
}

}
