/***************************************************************************
 *   Copyright (C) 2007-2008 by Nicolas Botti                              *
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

#include <iostream>

#include "wavelet2d.h"
#include "bindct.h"

using namespace std;

namespace rududu {

#define XI		1.149604398f
// #define ALPHA	(-3.f/2.f)
// #define BETA	(-1.f/16.f)
// #define GAMMA	(4.f/5.f)
// #define DELTA	(15.f/32.f)

#define SQRT2	1.414213562f

CWavelet2D::CWavelet2D(int x, int y, int level, int level_chg, int Align):
	pLow(0),
	pHigh(0),
	DimX(x),
	DimY(y)
{
	Init(level, level_chg, Align);
}

CWavelet2D::CWavelet2D(int x, int y, int level, int level_chg, CWavelet2D * pHigh, int Align):
	pLow(0),
	pHigh(0),
	DimX(x),
	DimY(y)
{
	this->pHigh = pHigh;
	pHigh->DBand.pParent = &DBand;
	DBand.pChild = &pHigh->DBand;
	pHigh->HBand.pParent = &HBand;
	HBand.pChild = &pHigh->HBand;
	pHigh->VBand.pParent = &VBand;
	VBand.pChild = &pHigh->VBand;

	Init(level, level_chg, Align);
}

CWavelet2D::~CWavelet2D()
{
	delete pLow;
}

void CWavelet2D::Init(int level, int level_chg, int Align){
	band_t type = sshort;
	if (level <= level_chg)
		type = sint;
	DBand.Init(type, DimX >> 1, DimY >> 1, Align);
	VBand.Init(type, DimX >> 1, (DimY + 1) >> 1, Align);
	HBand.Init(type, (DimX + 1) >> 1, DimY >> 1, Align);
	if (level > 1){
		pLow = new CWavelet2D((DimX + 1) >> 1, (DimY + 1) >> 1, level - 1, level_chg, this, Align);
	}else{
		LBand.Init(type, (DimX + 1) >> 1, (DimY + 1) >> 1, Align);
	}
}

void CWavelet2D::CodeBand(CMuxCodec * pCodec, int Quant, int lambda)
{
	CWavelet2D * pCurWav = this;

#ifdef GENERATE_HUFF_STATS
	cin.peek();
	if (cin.eof()) {
		for( int i = 0; i < 17; i++){
			for( int j = 0; j < 17; j++){
				CBandCodec::histo_l[i][j] = 0;
				if (j != 16) CBandCodec::histo_h[i][j] = 0;
			}
		}
	} else {
		for( int i = 0; i < 17; i++){
			for( int j = 0; j < 17; j++){
				cin >> CBandCodec::histo_l[i][j];
			}
		}
		for( int i = 0; i < 17; i++){
			for( int j = 0; j < 16; j++){
				cin >> CBandCodec::histo_h[i][j];
			}
		}
	}
#endif

	if (DBand.type == sshort) {
		DBand.buildTree<true, short>(Quant, lambda);
		HBand.buildTree<true, short>(Quant, lambda);
		VBand.buildTree<true, short>(Quant, lambda);
	} else if (DBand.type == sint) {
		DBand.buildTree<true, int>(Quant, lambda);
		HBand.buildTree<true, int>(Quant, lambda);
		VBand.buildTree<true, int>(Quant, lambda);
	}
	while( pCurWav->pLow ) pCurWav = pCurWav->pLow;
	if (pCurWav->LBand.type == sshort) {
		pCurWav->LBand.TSUQ<short>(Quant, 0.5f);
		pCurWav->LBand.pred<encode, short>(pCodec);
	} else if (pCurWav->LBand.type == sint) {
		pCurWav->LBand.TSUQ<int>(Quant, 0.5f);
		pCurWav->LBand.pred<encode, int>(pCodec);
	}
	while( pCurWav->pHigh ) {
		if (pCurWav->DBand.type == sshort) {
			if (pCurWav->pLow && pCurWav->pLow->DBand.type == sshort || pCurWav->pLow == 0) {
				pCurWav->VBand.tree<encode, false, short, short>(pCodec);
				pCurWav->HBand.tree<encode, false, short, short>(pCodec);
				pCurWav->DBand.tree<encode, false, short, short>(pCodec);
			} else if (pCurWav->pLow && pCurWav->pLow->DBand.type == sint) {
				pCurWav->VBand.tree<encode, false, short, int>(pCodec);
				pCurWav->HBand.tree<encode, false, short, int>(pCodec);
				pCurWav->DBand.tree<encode, false, short, int>(pCodec);
			}
		} else if (pCurWav->DBand.type == sint) {
			pCurWav->VBand.tree<encode, false, int, int>(pCodec);
			pCurWav->HBand.tree<encode, false, int, int>(pCodec);
			pCurWav->DBand.tree<encode, false, int, int>(pCodec);
		}
		pCurWav = pCurWav->pHigh;
	}
	if (pCurWav->DBand.type == sshort) {
		if (pCurWav->pLow && pCurWav->pLow->DBand.type == sshort || pCurWav->pLow == 0) {
			pCurWav->VBand.tree<encode, true, short, short>(pCodec);
			pCurWav->HBand.tree<encode, true, short, short>(pCodec);
			pCurWav->DBand.tree<encode, true, short, short>(pCodec);
		} else if (pCurWav->pLow && pCurWav->pLow->DBand.type == sint) {
			pCurWav->VBand.tree<encode, true, short, int>(pCodec);
			pCurWav->HBand.tree<encode, true, short, int>(pCodec);
			pCurWav->DBand.tree<encode, true, short, int>(pCodec);
		}
	} else if (pCurWav->DBand.type == sint) {
		pCurWav->VBand.tree<encode, true, int, int>(pCodec);
		pCurWav->HBand.tree<encode, true, int, int>(pCodec);
		pCurWav->DBand.tree<encode, true, int, int>(pCodec);
	}

#ifdef GENERATE_HUFF_STATS
	for( int i = 0; i < 17; i++){
		for( int j = 0; j < 17; j++){
			cout << CBandCodec::histo_l[i][j] << " ";
		}
		cout << endl;
	}
	cout << endl;
	for( int i = 0; i < 17; i++){
		for( int j = 0; j < 16; j++){
			cout << CBandCodec::histo_h[i][j] << " ";
		}
		cout << endl;
	}
	cout << endl;
#endif
}

void CWavelet2D::DecodeBand(CMuxCodec * pCodec)
{
	CWavelet2D * pCurWav = this;

	while( pCurWav->pLow ) pCurWav = pCurWav->pLow;
	if (pCurWav->LBand.type == sshort) {
		pCurWav->LBand.pred<decode, short>(pCodec);
	} else if (pCurWav->LBand.type == sint) {
		pCurWav->LBand.pred<decode, int>(pCodec);
	}
	while( pCurWav->pHigh ) {
		if (pCurWav->DBand.type == sshort) {
			if (pCurWav->pLow && pCurWav->pLow->DBand.type == sshort || pCurWav->pLow == 0) {
				pCurWav->VBand.tree<decode, false, short, short>(pCodec);
				pCurWav->HBand.tree<decode, false, short, short>(pCodec);
				pCurWav->DBand.tree<decode, false, short, short>(pCodec);
			} else if (pCurWav->pLow && pCurWav->pLow->DBand.type == sint) {
				pCurWav->VBand.tree<decode, false, short, int>(pCodec);
				pCurWav->HBand.tree<decode, false, short, int>(pCodec);
				pCurWav->DBand.tree<decode, false, short, int>(pCodec);
			}
		} else if (pCurWav->DBand.type == sint) {
			pCurWav->VBand.tree<decode, false, int, int>(pCodec);
			pCurWav->HBand.tree<decode, false, int, int>(pCodec);
			pCurWav->DBand.tree<decode, false, int, int>(pCodec);
		}
		pCurWav = pCurWav->pHigh;
	}
	if (pCurWav->DBand.type == sshort) {
		if (pCurWav->pLow && pCurWav->pLow->DBand.type == sshort || pCurWav->pLow == 0) {
			pCurWav->VBand.tree<decode, true, short, short>(pCodec);
			pCurWav->HBand.tree<decode, true, short, short>(pCodec);
			pCurWav->DBand.tree<decode, true, short, short>(pCodec);
		} else if (pCurWav->pLow && pCurWav->pLow->DBand.type == sint) {
			pCurWav->VBand.tree<decode, true, short, int>(pCodec);
			pCurWav->HBand.tree<decode, true, short, int>(pCodec);
			pCurWav->DBand.tree<decode, true, short, int>(pCodec);
		}
	} else if (pCurWav->DBand.type == sint) {
		pCurWav->VBand.tree<decode, true, int, int>(pCodec);
		pCurWav->HBand.tree<decode, true, int, int>(pCodec);
		pCurWav->DBand.tree<decode, true, int, int>(pCodec);
	}
}

unsigned int CWavelet2D::TSUQ(int Quant, float Thres)
{
	unsigned int Count = 0;
	if (DBand.type == sshort) {
		Count += DBand.TSUQ<short>(Quant, Thres);
		Count += HBand.TSUQ<short>(Quant, Thres);
		Count += VBand.TSUQ<short>(Quant, Thres);
	} else if (DBand.type == sint) {
		Count += DBand.TSUQ<int>(Quant, Thres);
		Count += HBand.TSUQ<int>(Quant, Thres);
		Count += VBand.TSUQ<int>(Quant, Thres);
	}

	if (pLow != 0) {
		Count += pLow->TSUQ(Quant, Thres);
	} else {
		if (LBand.type == sshort)
			Count += LBand.TSUQ<short>(Quant, 0.5f);
		else if (LBand.type == sint)
			Count += LBand.TSUQ<int>(Quant, 0.5f);
	}
	return Count;
}

void CWavelet2D::TSUQi(int Quant)
{
	if (DBand.type == sshort) {
		DBand.TSUQi<short>(Quant);
		HBand.TSUQi<short>(Quant);
		VBand.TSUQi<short>(Quant);
	} else if (DBand.type == sint) {
		DBand.TSUQi<int>(Quant);
		HBand.TSUQi<int>(Quant);
		VBand.TSUQi<int>(Quant);
	}

	if (pLow != 0) {
		pLow->TSUQi(Quant);
	} else {
		if (LBand.type == sshort)
			LBand.TSUQi<short>(Quant);
		else if (LBand.type == sint)
			LBand.TSUQi<int>(Quant);
	}
}

#define PRINT_STAT(band) \
	cout << band << " :\t"; \
	/*cout << "Moyenne : " << Mean << endl;*/ \
	cout << Var << endl;

void CWavelet2D::Stats(void)
{
	float Mean = 0, Var = 0;
	if (DBand.type == sshort) {
		DBand.Mean<short>(Mean, Var);
		PRINT_STAT("D");
		HBand.Mean<short>(Mean, Var);
		PRINT_STAT("H");
		VBand.Mean<short>(Mean, Var);
		PRINT_STAT("V");
	} else if (DBand.type == sint) {
		DBand.Mean<int>(Mean, Var);
		PRINT_STAT("D");
		HBand.Mean<int>(Mean, Var);
		PRINT_STAT("H");
		VBand.Mean<int>(Mean, Var);
		PRINT_STAT("V");
	}

	if (pLow != 0)
		pLow->Stats();
	else{
		if (LBand.type == sshort)
			LBand.Mean<short>(Mean, Var);
		else if (LBand.type == sint)
			LBand.Mean<int>(Mean, Var);
		PRINT_STAT("L");
	}
}

// #define MUL_FASTER

template <class C>
	inline C CWavelet2D::mult08(C a)
{
#ifdef MUL_FASTER
	// FIXME issue with int mult
	return (a * 0xCCCCu) >> 16;
#else
	a -= a >> 2;
	a += a >> 4;
	return a + (a >> 8);
#endif
}

template <class C>
	void CWavelet2D::TransLine97(C * i, int len)
{
	C * iend = i + len - 5;

	C tmp = i[0] + i[2];
	i[1] -= tmp + (tmp >> 1);
	i[0] -= i[1] >> 3;

	tmp = i[2] + i[4];
	i[3] -= tmp + (tmp >> 1);
	i[2] -= (i[1] + i[3]) >> 4;
	i[1] += mult08(i[0] + i[2]);
	i[0] += i[1] - (i[1] >> 4);

	i++;

	for( ; i < iend; i += 2) {
		tmp = i[3] + i[5];
		i[4] -= tmp + (tmp >> 1);
		i[3] -= (i[2] + i[4]) >> 4;
		i[2] += mult08(i[1] + i[3]);
		tmp = i[0] + i[2];
		i[1] += (tmp >> 1) - (tmp >> 5);
	}

	if (len & 1) {
		i[3] -= i[2] >> 3;
		i[2] += mult08(i[1] + i[3]);
		tmp = i[0] + i[2];
		i[1] += (tmp >> 1) - (tmp >> 5);

		i[3] += i[2] - (i[2] >> 4);
	} else {
		i[4] -= i[3] * 2 + i[3];
		i[3] -= (i[2] + i[4]) >> 4;
		i[2] += mult08(i[1] + i[3]);
		tmp = i[0] + i[2];
		i[1] += (tmp >> 1) - (tmp >> 5);

		i[4] += 2 * mult08(i[3]);
		tmp = i[2] + i[4];
		i[3] += (tmp >> 1) - (tmp >> 5);
	}
}

template <class C>
	void CWavelet2D::TransLine97I(C * i, int len)
{
	C * iend = i + len - 5;

	i[0] -= i[1] - (i[1] >> 4);

	C tmp = i[1] + i[3];
	i[2] -= (tmp >> 1) - (tmp >> 5);
	i[1] -= mult08(i[0] + i[2]);
	i[0] += i[1] >> 3;

	for( ; i < iend; i += 2) {
		tmp = i[3] + i[5];
		i[4] -= (tmp >> 1) - (tmp >> 5);
		i[3] -= mult08(i[2] + i[4]);
		i[2] += (i[1] + i[3]) >> 4;
		tmp = i[0] + i[2];
		i[1] += tmp + (tmp >> 1);
	}

	if (len & 1) {
		i[4] -= i[3] - (i[3] >> 4);
		i[3] -= mult08(i[2] + i[4]);
		i[2] += (i[1] + i[3]) >> 4;
		tmp = i[0] + i[2];
		i[1] += tmp + (tmp >> 1);

		i[4] += i[3] >> 3;
		tmp = i[2] + i[4];
		i[3] += tmp + (tmp >> 1);
	} else {
		i[3] -= 2 * mult08(i[2]);
		i[2] += (i[1] + i[3]) >> 4;
		tmp = i[0] + i[2];
		i[1] += tmp + (tmp >> 1);

		i[3] += i[2] * 2 + i[2];
	}
}

template <class C>
	void CWavelet2D::Transform97(C * pImage, int Stride)
{
	C * i[6];
	i[0] = pImage;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	C * out[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = (C*) LBand.pBand;
		out_stride[0] = LBand.DimXAlign;
	}

	for( int j = 0; j < 5; j++)
		TransLine97(i[j], DimX);

	for(int k = 0 ; k < DimX; k++) {
		C tmp = i[0][k] + i[2][k];
		i[1][k] -= tmp + (tmp >> 1);
		i[0][k] -= i[1][k] >> 3;

		tmp = i[2][k] + i[4][k];
		i[3][k] -= tmp + (tmp >> 1);
		i[2][k] -= (i[1][k] + i[3][k]) >> 4;
		i[1][k] += mult08(i[0][k] + i[2][k]);
		i[0][k] += i[1][k] - (i[1][k] >> 4);
		out[k & 1][k >> 1] = i[0][k];
	}

	for( int j = 0; j < 6; j++)
		i[j] += Stride;

	out[0] += out_stride[0];
	out[1] += out_stride[1];

	for( int j = 6; j < DimY; j += 2 ) {

		TransLine97(i[4], DimX);
		TransLine97(i[5], DimX);

		for(int k = 0 ; k < DimX; k++) {
			C tmp = i[3][k] + i[5][k];
			i[4][k] -= tmp + (tmp >> 1);
			i[3][k] -= (i[2][k] + i[4][k]) >> 4;
			i[2][k] += mult08(i[1][k] + i[3][k]);
			tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 1) - (tmp >> 5);
			out[2 + (k & 1)][k >> 1] = i[0][k];
			out[k & 1][k >> 1] = i[1][k];
		}

		for( int k = 0; k < 6; k++)
			i[k] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			out[k] += out_stride[k];
	}

	if (DimY & 1) {
		for(int k = 0 ; k < DimX; k++) {
			i[3][k] -= i[2][k] >> 3;
			i[2][k] += mult08(i[1][k] + i[3][k]);
			C tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 1) - (tmp >> 5);

			i[3][k] += i[2][k] - (i[2][k] >> 4);

			tmp = k & 1;
			out[tmp][k >> 1] = i[1][k];
			out[2 + tmp][k >> 1] = i[0][k];
			out[tmp][out_stride[tmp] + (k >> 1)] = i[3][k];
			out[2 + tmp][out_stride[2 + tmp] + (k >> 1)] = i[2][k];
		}
	} else {
		TransLine97(i[4], DimX);
		for(int k = 0 ; k < DimX; k++) {
			i[4][k] -= i[3][k] * 2 + i[3][k];
			i[3][k] -= (i[2][k] + i[4][k]) >> 4;
			i[2][k] += mult08(i[1][k] + i[3][k]);
			C tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 1) - (tmp >> 5);

			i[4][k] += 2 * mult08(i[3][k]);
			tmp = i[2][k] + i[4][k];
			i[3][k] += (tmp >> 1) - (tmp >> 5);

			tmp = k & 1;
			out[tmp][k >> 1] = i[1][k];
			out[2 + tmp][k >> 1] = i[0][k];
			out[tmp][out_stride[tmp] + (k >> 1)] = i[3][k];
			out[2 + tmp][out_stride[2 + tmp] + (k >> 1)] = i[2][k];
			out[2 + tmp][out_stride[2 + tmp] * 2 + (k >> 1)] = i[4][k];
		}
	}
}

template <class C>
	void CWavelet2D::Transform97I(C * pImage, int Stride)
{
	C * in[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = (C*) LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	C * i[6];
	i[0] = pImage - DimY * Stride;
	if (pHigh != 0) i[0] += Stride - DimX;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		C tmp = k & 1;
		i[0][k] = in[tmp][k >> 1];
		i[1][k] = in[2 + tmp][k >> 1];
		i[2][k] = in[tmp][in_stride[tmp] + (k >> 1)];
		i[3][k] = in[2 + tmp][in_stride[2 + tmp] + (k >> 1)];

		i[0][k] -= i[1][k] - (i[1][k] >> 4);

		tmp = i[1][k] + i[3][k];
		i[2][k] -= (tmp >> 1) - (tmp >> 5);
		i[1][k] -= mult08(i[0][k] + i[2][k]);
		i[0][k] += i[1][k] >> 3;
	}

	for( int k = 0; k < 4; k++)
		in[k] += 2 * in_stride[k];

	for( int j = 5; j < DimY; j += 2 ){
		for(int k = 0 ; k < DimX; k++) {
			i[4][k] = in[k & 1][k >> 1];
			i[5][k] = in[2 + (k & 1)][k >> 1];

			C tmp = i[3][k] + i[5][k];
			i[4][k] -= (tmp >> 1) - (tmp >> 5);
			i[3][k] -= mult08(i[2][k] + i[4][k]);
			i[2][k] += (i[1][k] + i[3][k]) >> 4;
			tmp = i[0][k] + i[2][k];
			i[1][k] += tmp + (tmp >> 1);
		}

		TransLine97I(i[0], DimX);
		TransLine97I(i[1], DimX);

		for( int k = 0; k < 6; k++)
			i[k] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			in[k] += in_stride[k];
	}

	if (DimY & 1) {
		for(int k = 0 ; k < DimX; k++) {
			i[4][k] = in[k & 1][k >> 1];

			i[4][k] -= i[3][k] - (i[3][k] >> 4);
			i[3][k] -= mult08(i[2][k] + i[4][k]);
			i[2][k] += (i[1][k] + i[3][k]) >> 4;
			C tmp = i[0][k] + i[2][k];
			i[1][k] += tmp + (tmp >> 1);

			i[4][k] += i[3][k] >> 3;
			tmp = i[2][k] + i[4][k];
			i[3][k] += tmp + (tmp >> 1);
		}
		TransLine97I(i[4], DimX);
	} else {
		for(int k = 0 ; k < DimX; k++) {
			i[3][k] -= 2 * mult08(i[2][k]);
			i[2][k] += (i[1][k] + i[3][k]) >> 4;
			C tmp = i[0][k] + i[2][k];
			i[1][k] += tmp + (tmp >> 1);

			i[3][k] += i[2][k] * 2 + i[2][k];
		}
	}

	for( int j = 0; j < 4; j++)
		TransLine97I(i[j], DimX);
}

template <class C>
	void CWavelet2D::TransLine53(C * i, int len)
{
	C * iend = i + len - 3;

	i[1] -= (i[0] + i[2]) >> 1;
	i[0] += i[1] >> 1;

	i++;

	for( ; i < iend; i += 2) {
		i[2] -= (i[1] + i[3]) >> 1;
		i[1] += (i[0] + i[2]) >> 2;
	}

	if (len & 1) {
		i[1] += i[0] >> 1;
	} else {
		i[2] -= i[1];
		i[1] += (i[0] + i[2]) >> 2;
	}
}

template <class C>
	void CWavelet2D::TransLine53I(C * i, int len)
{
	C * iend = i + len - 3;

	i[0] -= i[1] >> 1;

	for( ; i < iend; i += 2) {
		i[2] -= (i[1] + i[3]) >> 2;
		i[1] += (i[0] + i[2]) >> 1;
	}

	if (len & 1) {
		i[2] -= i[1] >> 1;
		i[1] += (i[0] + i[2]) >> 1;
	} else {
		i[1] += i[0];
	}
}

template <class C>
	void CWavelet2D::Transform53(C * pImage, int Stride)
{
	C * i[4];
	i[0] = pImage;
	for( int j = 1; j < 4; j++)
		i[j] = i[j-1] + Stride;

	C * out[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = (C*) LBand.pBand;
		out_stride[0] = LBand.DimXAlign;
	}

	for( int j = 0; j < 3; j++)
		TransLine53(i[j], DimX);

	for(int k = 0 ; k < DimX; k++) {
		i[1][k] -= (i[0][k] + i[2][k]) >> 1;
		i[0][k] += i[1][k] >> 1;
		out[k & 1][k >> 1] = i[0][k];
	}

	for( int j = 0; j < 4; j++)
		i[j] += Stride;

	out[0] += out_stride[0];
	out[1] += out_stride[1];

	for( int j = 4; j < DimY; j += 2 ) {

		TransLine53(i[2], DimX);
		TransLine53(i[3], DimX);

		for(int k = 0 ; k < DimX; k++) {
			i[2][k] -= (i[1][k] + i[3][k]) >> 1;
			i[1][k] += (i[0][k] + i[2][k]) >> 2;
			out[2 + (k & 1)][k >> 1] = i[0][k];
			out[k & 1][k >> 1] = i[1][k];
		}

		for( int k = 0; k < 4; k++)
			i[k] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			out[k] += out_stride[k];
	}

	if (DimY & 1) {
		for(int k = 0 ; k < DimX; k++) {
			i[1][k] += i[0][k] >> 1;
			out[2 + (k & 1)][k >> 1] = i[0][k];
			out[k & 1][k >> 1] = i[1][k];
		}
	} else {
		TransLine53(i[2], DimX);
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] -= i[1][k];
			i[1][k] += (i[0][k] + i[2][k]) >> 2;
			out[2 + (k & 1)][k >> 1] = i[0][k];
			out[k & 1][k >> 1] = i[1][k];
			out[2 + (k & 1)][out_stride[2 + (k & 1)] + (k >> 1)] = i[2][k];
		}
	}
}

template <class C>
	void CWavelet2D::Transform53I(C * pImage, int Stride)
{
	C * in[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = (C*) LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	C * i[4];
	i[0] = pImage - DimY * Stride;
	if (pHigh != 0) i[0] += Stride - DimX;
	for( int j = 1; j < 4; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		i[0][k] = in[k & 1][k >> 1];
		i[1][k] = in[2 + (k & 1)][k >> 1];
		i[0][k] -= i[1][k] >> 1;
	}

	for( int k = 0; k < 4; k++)
		in[k] += in_stride[k];

	for( int j = 3; j < DimY; j += 2 ){
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] = in[k & 1][k >> 1];
			i[3][k] = in[2 + (k & 1)][k >> 1];

			i[2][k] -= (i[1][k] + i[3][k]) >> 2;
			i[1][k] += (i[0][k] + i[2][k]) >> 1;
		}

		TransLine53I(i[0], DimX);
		TransLine53I(i[1], DimX);

		for( int k = 0; k < 4; k++)
			i[k] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			in[k] += in_stride[k];
	}

	if (DimY & 1) {
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] = in[k & 1][k >> 1];

			i[2][k] -= i[1][k] >> 1;
			i[1][k] += (i[0][k] + i[2][k]) >> 1;
		}
		TransLine53I(i[2], DimX);
	} else {
		for(int k = 0 ; k < DimX; k++)
			i[1][k] += i[0][k];
	}

	TransLine53I(i[0], DimX);
	TransLine53I(i[1], DimX);
}

template <class C>
	void CWavelet2D::TransLine75(C * i, int len)
{
	C * iend = i + len - 4;

	i[0] -= i[1] >> 1;

	i[2] -= (i[1] + i[3]) >> 2;
	i[1] -= i[0] + i[2];
	i[0] += (i[1] >> 2) + (i[1] >> 3);

	i++;

	for( ; i < iend; i += 2) {
		i[3] -= (i[2] + i[4]) >> 2;
		i[2] -= i[1] + i[3];
		C tmp = i[0] + i[2];
		i[1] += (tmp >> 3) + (tmp >> 4);
	}

	if (len & 1) {
		i[3] -= i[2] >> 1;
		i[2] -= i[1] + i[3];
		C tmp = i[0] + i[2];
		i[1] += (tmp >> 3) + (tmp >> 4);

		i[3] += (i[2] >> 2) + (i[2] >> 3);
	} else {
		i[2] -= i[1] * 2;
		C tmp = i[0] + i[2];
		i[1] += (tmp >> 3) + (tmp >> 4);
	}
}

template <class C>
	void CWavelet2D::TransLine75I(C * i, int len)
{
	C * iend = i + len - 4;

	i[0] -= (i[1] >> 2) + (i[1] >> 3);

	C tmp = i[1] + i[3];
	i[2] -= (tmp >> 3) + (tmp >> 4);
	i[1] += i[0] + i[2];
	i[0] += i[1] >> 1;

	i++;

	for( ; i < iend; i += 2) {
		tmp = i[2] + i[4];
		i[3] -= (tmp >> 3) + (tmp >> 4);
		i[2] += i[1] + i[3];
		i[1] += (i[0] + i[2]) >> 2;
	}

	if (len & 1) {
		i[3] -= (i[2] >> 2) + (i[2] >> 3);
		i[2] += i[1] + i[3];
		i[1] += (i[0] + i[2]) >> 2;

		i[3] += i[2] >> 1;
	} else {
		i[2] += i[1] * 2;
		i[1] += (i[0] + i[2]) >> 2;
	}
}

template <class C>
	void CWavelet2D::Transform75(C * pImage, int Stride)
{
	C * i[5];
	i[0] = pImage;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	C * out[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = (C*) LBand.pBand;
		out_stride[0] = LBand.DimXAlign;
	}

	for( int j = 0; j < 4; j++)
		TransLine75(i[j], DimX);

	for(int k = 0 ; k < DimX; k++) {
		i[0][k] -= i[1][k] >> 1;

		i[2][k] -= (i[1][k] + i[3][k]) >> 2;
		i[1][k] -= i[0][k] + i[2][k];
		i[0][k] += (i[1][k] >> 2) + (i[1][k] >> 3);
		out[k & 1][k >> 1] = i[0][k];
	}

	for( int j = 0; j < 5; j++)
		i[j] += Stride;

	out[0] += out_stride[0];
	out[1] += out_stride[1];

	for( int j = 5; j < DimY; j += 2 ) {

		TransLine75(i[3], DimX);
		TransLine75(i[4], DimX);

		for(int k = 0 ; k < DimX; k++) {
			i[3][k] -= (i[2][k] + i[4][k]) >> 2;
			i[2][k] -= i[1][k] + i[3][k];
			C tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 3) + (tmp >> 4);
			out[2 + (k & 1)][k >> 1] = i[0][k];
			out[k & 1][k >> 1] = i[1][k];
		}

		for( int k = 0; k < 5; k++)
			i[k] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			out[k] += out_stride[k];
	}

	if (DimY & 1) {
		for(int k = 0 ; k < DimX; k++) {
			i[3][k] -= i[2][k] >> 1;
			i[2][k] -= i[1][k] + i[3][k];
			C tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 3) + (tmp >> 4);

			i[3][k] += (i[2][k] >> 2) + (i[2][k] >> 3);

			tmp = k & 1;
			out[tmp][k >> 1] = i[1][k];
			out[2 + tmp][k >> 1] = i[0][k];
			out[tmp][out_stride[tmp] + (k >> 1)] = i[3][k];
			out[2 + tmp][out_stride[2 + tmp] + (k >> 1)] = i[2][k];
		}
	} else {
		TransLine75(i[4], DimX);
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] -= i[1][k] * 2;
			C tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 3) + (tmp >> 4);

			tmp = k & 1;
			out[tmp][k >> 1] = i[1][k];
			out[2 + tmp][k >> 1] = i[0][k];
			out[2 + tmp][out_stride[2 + tmp] + (k >> 1)] = i[2][k];
		}
	}
}

template <class C>
	void CWavelet2D::Transform75I(C * pImage, int Stride)
{
	C * in[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = (C*) LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	C * i[5];
	i[0] = pImage - DimY * Stride;
	if (pHigh != 0) i[0] += Stride - DimX;
	for( int j = 1; j < 5; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		C tmp = k & 1;
		i[0][k] = in[tmp][k >> 1];
		i[1][k] = in[2 + tmp][k >> 1];
		i[2][k] = in[tmp][in_stride[tmp] + (k >> 1)];
		i[3][k] = in[2 + tmp][in_stride[2 + tmp] + (k >> 1)];

		i[0][k] -= (i[1][k] >> 2) + (i[1][k] >> 3);

		tmp = i[1][k] + i[3][k];
		i[2][k] -= (tmp >> 3) + (tmp >> 4);
		i[1][k] += i[0][k] + i[2][k];
		i[0][k] += i[1][k] >> 1;
	}

	TransLine75I(i[0], DimX);

	for( int j = 0; j < 5; j++)
		i[j] += Stride;

	for( int k = 0; k < 4; k++)
		in[k] += 2 * in_stride[k];

	for( int j = 5; j < DimY; j += 2 ){
		for(int k = 0 ; k < DimX; k++) {
			i[3][k] = in[k & 1][k >> 1];
			i[4][k] = in[2 + (k & 1)][k >> 1];

			C tmp = i[2][k] + i[4][k];
			i[3][k] -= (tmp >> 3) + (tmp >> 4);
			i[2][k] += i[1][k] + i[3][k];
			i[1][k] += (i[0][k] + i[2][k]) >> 2;
		}

		TransLine75I(i[0], DimX);
		TransLine75I(i[1], DimX);

		for( int k = 0; k < 5; k++)
			i[k] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			in[k] += in_stride[k];
	}

	if (DimY & 1) {
		for(int k = 0 ; k < DimX; k++) {
			i[3][k] = in[k & 1][k >> 1];

			i[3][k] -= (i[2][k] >> 2) + (i[2][k] >> 3);
			i[2][k] += i[1][k] + i[3][k];
			i[1][k] += (i[0][k] + i[2][k]) >> 2;

			i[3][k] += i[2][k] >> 1;
		}
		TransLine75I(i[3], DimX);
	} else {
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] += i[1][k] * 2;
			i[1][k] += (i[0][k] + i[2][k]) >> 2;
		}
	}

	for( int j = 0; j < 3; j++)
		TransLine75I(i[j], DimX);
}

template <class C>
	void CWavelet2D::TransLineHaar(C * i, int len)
{
	C * iend = i + len - 1;

	for( ; i < iend; i += 2) {
		i[1] -= i[0];
		i[0] += i[1] >> 1;
	}
}

template <class C>
	void CWavelet2D::TransLineHaarI(C * i, int len)
{
	C * iend = i + len - 1;

	for( ; i < iend; i += 2) {
		i[0] -= i[1] >> 1;
		i[1] += i[0];
	}
}

template <class C>
	void CWavelet2D::TransformHaar(C * pImage, int Stride)
{
	C * i[2];
	i[0] = pImage;
	i[1] = i[0] + Stride;

	C * out[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = (C*) LBand.pBand;
		out_stride[0] = LBand.DimXAlign;
	}

	for( int j = 0; j < DimY - 1; j += 2 ) {

		TransLineHaar(i[0], DimX);
		TransLineHaar(i[1], DimX);

		for(int k = 0 ; k < DimX; k++) {
			i[1][k] -= i[0][k];
			i[0][k] += i[1][k] >> 1;
			out[2 + (k & 1)][k >> 1] = i[1][k];
			out[k & 1][k >> 1] = i[0][k];
		}

		i[0] += 2 * Stride;
		i[1] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			out[k] += out_stride[k];
	}
}

template <class C>
	void CWavelet2D::TransformHaarI(C * pImage, int Stride)
{
	C * in[4] = {pImage, (C*) VBand.pBand, (C*) HBand.pBand, (C*) DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = (C*) LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	C * i[2];
	i[0] = pImage - DimY * Stride;
	if (pHigh != 0) i[0] += Stride - DimX;
	i[1] = i[0] + Stride;

	for( int j = 0; j < DimY - 1; j += 2 ){
		for(int k = 0 ; k < DimX; k++) {
			i[0][k] = in[k & 1][k >> 1];
			i[1][k] = in[2 + (k & 1)][k >> 1];

			i[0][k] -= i[1][k] >> 1;
			i[1][k] += i[0][k];
		}

		TransLineHaarI(i[0], DimX);
		TransLineHaarI(i[1], DimX);

		i[0] += 2 * Stride;
		i[1] += 2 * Stride;
		for( int k = 0; k < 4; k++)
			in[k] += in_stride[k];
	}
}

/*
template <bool forward>
	void CWavelet2D::DCT4H(void)
{
	short * pCur = HBand.pBand;
	for( unsigned int j = 0; j < HBand.DimY; j++) {
		for( unsigned int i = 0; i < HBand.DimX; i += 4) {
			short * x = pCur + i;

			if (forward) {
				BFLY_FWD(x[0], x[3]);
				BFLY_FWD(x[1], x[2]);

				x[0] += x[1];
				x[1] -= x[0] >> 1;

				x[2] -= P1(x[3]);
				x[3] -= U1(x[2]);
			} else {
				x[3] += U1(x[2]);
				x[2] += P1(x[3]);

				x[1] += x[0] >> 1;
				x[0] -= x[1];

				BFLY_INV(x[0], x[3]);
				BFLY_INV(x[1], x[2]);
			}
		}
		pCur += HBand.DimXAlign;
	}
}

template <bool forward>
	void CWavelet2D::DCT4V(void)
{
	int stride = VBand.DimXAlign;
	short * x[4] = {VBand.pBand, VBand.pBand + stride,
			VBand.pBand + 2 * stride, VBand.pBand + 3 * stride};

	for( unsigned int j = 0; j < VBand.DimY; j += 4) {
		for( unsigned int i = 0; i < VBand.DimX; i++) {
			if (forward) {
				BFLY_FWD(x[0][i], x[3][i]);
				BFLY_FWD(x[1][i], x[2][i]);

				x[0][i] += x[1][i];
				x[1][i] -= x[0][i] >> 1;

				x[2][i] -= P1(x[3][i]);
				x[3][i] -= U1(x[2][i]);
			} else {
				x[3][i] += U1(x[2][i]);
				x[2][i] += P1(x[3][i]);

				x[1][i] += x[0][i] >> 1;
				x[0][i] -= x[1][i];

				BFLY_INV(x[0][i], x[3][i]);
				BFLY_INV(x[1][i], x[2][i]);
			}
		}
		for( int i = 0; i < 4; i++){
			x[i] += stride * 4;
		}
	}
}
*/

template <class C>
	void CWavelet2D::Transform(C * pImage, int Stride, trans t)
{
	if (t == cdf97) {
		Transform97(pImage, Stride);
	} else if (t == cdf53) {
		Transform53(pImage, Stride);
	} else if (t == haar) {
		TransformHaar(pImage, Stride);
	} else if (t == cdf75)
		Transform75(pImage, Stride);

	if (pLow != 0) {
		if (DBand.type == sshort && pLow->DBand.type == sint) {
			C * pIn = pImage;
			for( int j = 0; j < pLow->DimY; j++){
				int * pOut = (int*) pIn;
				for( int i = pLow->DimX - 1; i >= 0; i--){
					pOut[i] = (int) pIn[i];
				}
				pIn += Stride;
			}
			Stride >>= 1;
		}
		if (pLow->DBand.type == sshort)
			pLow->Transform(pImage, Stride, t);
		else
			pLow->Transform((int*)pImage, Stride, t);
	}
}

template void CWavelet2D::Transform(short *, int, trans);

template <class C>
	void CWavelet2D::TransformI(C * pImage, int Stride, trans t)
{
	if (pLow != 0) {
		if (pLow->DBand.type == sshort)
			pLow->TransformI(pImage, Stride, t);
		else if (DBand.type == sshort)
			pLow->TransformI((int*)pImage, Stride >> 1, t);
		else
			pLow->TransformI((int*)pImage, Stride, t);

		if (DBand.type == sshort && pLow->DBand.type == sint) {
			C * pOut = pImage - (pLow->DimY - 1) * Stride;
			for( int j = 0; j < pLow->DimY; j++){
				int * pIn = (int*) pOut;
				for( int i = -1 ; i >= -pLow->DimX; i--){
					pOut[i] = (C) pIn[i];
				}
				pOut += Stride;
			}
		}
	}

	if (t == cdf97) {
		Transform97I(pImage, Stride);
	} else if (t == cdf53) {
		Transform53I(pImage, Stride);
	} else if (t == haar) {
		TransformHaarI(pImage, Stride);
	} else if (t == cdf75)
		Transform75I(pImage, Stride);
}

template void CWavelet2D::TransformI(short *, int, trans);

/*
template <bool forward>
	void CWavelet2D::DCT4(void)
{
	DCT4H<forward>();
	DCT4V<forward>();

	if (pLow != 0)
		pLow->DCT4<forward>();
}

template void CWavelet2D::DCT4<true>(void);
template void CWavelet2D::DCT4<false>(void);
*/

void CWavelet2D::SetWeight(trans t, float baseWeight)
{
	float scale;
	if (t == cdf97) {
		scale = XI * XI;
	} else if (t == cdf75) {
		scale = 8; // (2*SQRT2)^2
	} else {
		scale = 2; // (SQRT2)^2
	}

	if (pHigh != 0){
		DBand.Weight = pHigh->VBand.Weight;
		VBand.Weight = pHigh->LBand.Weight;
		HBand.Weight = VBand.Weight;
		LBand.Weight = VBand.Weight * scale;
	}else{
		DBand.Weight = baseWeight / scale;
		VBand.Weight = baseWeight;
		HBand.Weight = baseWeight;
		LBand.Weight = baseWeight * scale;
	}

	if (pLow != 0)
		pLow->SetWeight(t);
}

}
