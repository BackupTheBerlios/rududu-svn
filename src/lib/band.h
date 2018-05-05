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

#pragma once

#include <stdint.h>

namespace rududu {

#define LL_BAND		0
#define V_BAND		1
#define H_BAND		2
#define D1_BAND		3	// diag direction = "\"
#define D2_BAND		4	// diag direction = "/"

#define ALIGN	32

typedef enum band_t {sshort, sint};

class CBand
{
public:
	CBand(void);
	virtual ~CBand();

	unsigned int DimX;		// Width of the band (Datas)
	unsigned int DimY;		// Height of the band
	unsigned int DimXAlign;	// Alignement Width (Buffer)
	unsigned int BandSize;	// (DimXAlign * DimY), the band size in SAMPLES
	int Max;				// Max of the band
	int Min;				// Min of the band
	unsigned int Dist;		// Distortion (Square Error)
	unsigned int Count;		// Count of non-zero coeffs
	float Weight;			// Weighting of the band distortion
							// (used for Quant calc and PSNR calc)

	CBand *pParent;			// Parent Band
	CBand *pChild;			// Child Band
	CBand *pNeighbor[3];	// Band neighbors (other component bands
							// that can be used for context modeling)
	void *pBand;			// Band datas
	band_t type;			// type of the datas

	void Init(band_t type = sshort, unsigned int x = 0, unsigned int y = 0, int Align = ALIGN);

	// Quantification

	template <class C>
		unsigned int TSUQ(int Quant, float Thres)
	{
		int Diff = DimXAlign - DimX;
		Quant = (int) (Quant / Weight);
		if (Quant == 0) Quant = 1;
		int iQuant = (int) (1 << 16) / Quant;
		C T = (C) (Thres * Quant);
		int Min = 0, Max = 0;
		Count = 0;
		C * pBand = (C*) this->pBand;
		for ( unsigned int j = 0, n = 0; j < DimY ; j++ ) {
			for ( unsigned int nEnd = n + DimX; n < nEnd ; n++ ) {
				if ( U(pBand[n] + T) <= U(2 * T)) {
					pBand[n] = 0;
				} else {
					Count++;
					pBand[n] = (pBand[n] * iQuant + (1 << 15)) >> 16;
					if (pBand[n] > Max) Max = pBand[n];
					if (pBand[n] < Min) Min = pBand[n];
				}
			}
			n += Diff;
		}
		this->Min = Min;
		this->Max = Max;
		return Count;
	}

	template <class C>
		void TSUQi(C Quant)
	{
		int Diff = DimXAlign - DimX;
		Quant = (C) (Quant / Weight);
		C * pBand = (C*) this->pBand;
		if (Quant == 0) Quant = 1;
		for ( unsigned int j = 0, n = 0; j < DimY ; j ++ ) {
			for ( unsigned int nEnd = n + DimX; n < nEnd ; n++ ) {
				pBand[n] *= Quant;
			}
			n += Diff;
		}
	}
	/*
	unsigned int TSUQ_DCTH(short Quant, float Thres);
	unsigned int TSUQ_DCTV(short Quant, float Thres);
	void TSUQ_DCTHi(short Quant);
	void TSUQ_DCTVi(short Quant);
	*/

	// Statistiques
	template <class C>
		void Mean( float & Mean, float & Var )
	{
		int64_t Sum = 0;
		int64_t SSum = 0;
		C * pBand = (C*) this->pBand;
		for ( unsigned int j = 0; j < DimY; j++ ) {
			unsigned int J = j * DimXAlign;
			for ( unsigned int i = 0; i < DimX; i++ ) {
				Sum += pBand[i + J];
				SSum += pBand[i + J] * pBand[i + J];
			}
		}
		Mean = (float) Sum * Weight / ( DimX * DimY );
		Var = ((float) (SSum - Sum * Sum)) * Weight * Weight /
			(( DimX * DimY ) * ( DimX * DimY ));
	}

	// Utilitaires
	template <class C>
		void Add( C val )
	{
		C * pBand = (C*) this->pBand;
		for ( unsigned int i = 0; i < BandSize; i++ )
			pBand[i] += val;
	}

	void Clear(bool recurse = false);

	template <class C, class T> void GetBand(T * pOut)
	{
		C * pIn = (C*) pBand;
		int add = 1 << (sizeof(T) * 8 - 1);
		for( unsigned int j = 0; j < DimY; j++){
			for( unsigned int i = 0; i < DimX; i++)
				pOut[i] = (T)(pIn[i] + add);
			pOut += DimX;
			pIn += DimXAlign;
		}
	}

private:
	char * pData;

	int GetSampleSize(void);
};

}
