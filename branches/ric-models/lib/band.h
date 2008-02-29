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

namespace rududu {

#define LL_BAND		0
#define V_BAND		1
#define H_BAND		2
#define D1_BAND		3	// diag direction = "\"
#define D2_BAND		4	// diag direction = "/"

#define ALIGN	32

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
	short *pBand;			// Band datas

	void Init(unsigned int x = 0, unsigned int y = 0, int Align = ALIGN);

	// Quantification
	unsigned int TSUQ(short Quant, float Thres);
	void TSUQi(short Quant);
	unsigned int TSUQ_DCTH(short Quant, float Thres);
	unsigned int TSUQ_DCTV(short Quant, float Thres);
	void TSUQ_DCTHi(short Quant);
	void TSUQ_DCTVi(short Quant);

	// Statistiques
	void Mean(float & Mean, float & Var);

	// Utilitaires
	void Add(short val);
	void Clear(bool recurse = false);
	template <class T> void GetBand(T * pOut);

private:
	char * pData;
};

}
