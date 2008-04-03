/***************************************************************************
 *   Copyright (C) 2006-2008 by Nicolas Botti                              *
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

#include <string.h>

#include "band.h"

namespace rududu {

CBand::CBand( void ):
pData(0)
{
	pParent = 0;
	pChild = 0;
	Init();
}

CBand::~CBand()
{
	delete[] pData;
}

int CBand::GetSampleSize(void)
{
	switch(type){
	case sshort :
		return sizeof(short);
	case sint :
		return sizeof(int);
	}
	return 0;
}

void CBand::Init(band_t type, unsigned int x, unsigned int y, int Align)
{
	this->type = type;
	int sample_size = GetSampleSize();
	DimX = x;
	DimY = y;
	DimXAlign = (( DimX * sample_size + Align - 1 ) & ( -Align )) / sample_size;
	BandSize = DimXAlign * DimY;
	Weight = 1;
	Count = 0;
	if (BandSize != 0){
		pData = new char[BandSize * sample_size + Align];
		pBand = (void*)(((intptr_t)pData + Align - 1) & (-Align));
	}
}

/*

// {1, 2, sin(3*pi/8)/sqrt(2), 1/(sqrt(2)*sin(3*pi/8))}
static const float dct_norm[4] = {1.f, 2.f, 0.6532814824381882515f, 0.7653668647301795581f};

unsigned int CBand::TSUQ_DCTH(short Quant, float Thres)
{
	short Q[4], T[4];
	int iQ[4];
	for( int i = 0; i < 4; i++) {
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
		if (Q[i] == 0) Q[i] = 1;
		iQ[i] = (int) (1 << 16) / Q[i];
		T[i] = (short) (Thres * Q[i]);
	}
	int Min = 0, Max = 0;
	Count = 0;
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++) {
			if ( (unsigned short) (pCur[i] + T[i & 3]) <= (unsigned short) (2 * T[i & 3])) {
				pCur[i] = 0;
			} else {
				Count++;
				pCur[i] = (pCur[i] * iQ[i & 3] + (1 << 15)) >> 16;
				Max = MAX(pCur[i], Max);
				Min = MIN(pCur[i], Min);
			}
		}
		pCur += DimXAlign;
	}
	this->Min = Min;
	this->Max = Max;
	return Count;
}

unsigned int CBand::TSUQ_DCTV(short Quant, float Thres)
{
	short Q[4], T[4];
	int iQ[4];
	for( int i = 0; i < 4; i++) {
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
		if (Q[i] == 0) Q[i] = 1;
		iQ[i] = (int) (1 << 16) / Q[i];
		T[i] = (short) (Thres * Q[i]);
	}
	int Min = 0, Max = 0;
	Count = 0;
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++) {
			if ( (unsigned short) (pCur[i] + T[j & 3]) <= (unsigned short) (2 * T[j & 3])) {
				pCur[i] = 0;
			} else {
				Count++;
				pCur[i] = (pCur[i] * iQ[j & 3] + (1 << 15)) >> 16;
				Max = MAX(pCur[i], Max);
				Min = MIN(pCur[i], Min);
			}
		}
		pCur += DimXAlign;
	}
	this->Min = Min;
	this->Max = Max;
	return Count;
}

void CBand::TSUQ_DCTHi(short Quant)
{
	short Q[4];
	for( int i = 0; i < 4; i++)
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++)
			pCur[i] *= Q[i & 3];
		pCur += DimXAlign;
	}
}

void CBand::TSUQ_DCTVi(short Quant)
{
	short Q[4];
	for( int i = 0; i < 4; i++)
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++)
			pCur[i] *= Q[j & 3];
		pCur += DimXAlign;
	}
}

*/

void CBand::Clear(bool recurse)
{
	memset(pBand, 0, BandSize * GetSampleSize());
	if (recurse && pParent != 0)
		pParent->Clear(true);
}

}
