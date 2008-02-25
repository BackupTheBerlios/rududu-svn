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

CWavelet2D::CWavelet2D(int x, int y, int level, int Align):
	pLow(0),
	pHigh(0),
	DimX(x),
	DimY(y)
{
	Init(level, Align);
}

CWavelet2D::CWavelet2D(int x, int y, int level, CWavelet2D * pHigh, int Align):
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

	Init(level, Align);
}

CWavelet2D::~CWavelet2D()
{
	delete pLow;
}

void CWavelet2D::Init(int level, int Align){
	DBand.Init(DimX >> 1, DimY >> 1, Align);
	VBand.Init(DimX >> 1, (DimY + 1) >> 1, Align);
	HBand.Init((DimX + 1) >> 1, DimY >> 1, Align);
	if (level > 1){
		pLow = new CWavelet2D((DimX + 1) >> 1, (DimY + 1) >> 1, level - 1, this, Align);
	}else{
		LBand.Init((DimX + 1) >> 1, (DimY + 1) >> 1, Align);
	}
}

void CWavelet2D::CodeBand(CMuxCodec * pCodec, int method,
						  short Quant, int lambda)
{
	CWavelet2D * pCurWav = this;

	switch( method ){
		case 1 :
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
			pCurWav->DBand.buildTree<true>(Quant, lambda);
			pCurWav->HBand.buildTree<true>(Quant, lambda);
			pCurWav->VBand.buildTree<true>(Quant, lambda);
			while( pCurWav->pLow ) pCurWav = pCurWav->pLow;
			pCurWav->LBand.TSUQ(Quant, 0.5f);
			pCurWav->LBand.pred<encode>(pCodec);
			while( pCurWav->pHigh ) {
				pCurWav->VBand.tree<encode, false>(pCodec);
				pCurWav->HBand.tree<encode, false>(pCodec);
				pCurWav->DBand.tree<encode, false>(pCodec);
				pCurWav = pCurWav->pHigh;
			}
			pCurWav->VBand.tree<encode, true>(pCodec);
			pCurWav->HBand.tree<encode, true>(pCodec);
			pCurWav->DBand.tree<encode, true>(pCodec);
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
}

void CWavelet2D::DecodeBand(CMuxCodec * pCodec, int method)
{
	CWavelet2D * pCurWav = this;

	switch( method ){
		case 1 :
			while( pCurWav->pLow ) pCurWav = pCurWav->pLow;
			pCurWav->LBand.pred<decode>(pCodec);
			while( pCurWav->pHigh ) {
				pCurWav->VBand.Clear(false);
				pCurWav->VBand.tree<decode, false>(pCodec);
				pCurWav->HBand.Clear(false);
				pCurWav->HBand.tree<decode, false>(pCodec);
				pCurWav->DBand.Clear(false);
				pCurWav->DBand.tree<decode, false>(pCodec);
				pCurWav = pCurWav->pHigh;
			}
			pCurWav->VBand.Clear(false);
			pCurWav->VBand.tree<decode, true>(pCodec);
			pCurWav->HBand.Clear(false);
			pCurWav->HBand.tree<decode, true>(pCodec);
			pCurWav->DBand.Clear(false);
			pCurWav->DBand.tree<decode, true>(pCodec);
	}
}

template <bool use_dct>
	unsigned int CWavelet2D::TSUQ(short Quant, float Thres)
{
	unsigned int Count = 0;
	Count += DBand.TSUQ(Quant, Thres);
	if (use_dct) {
		Count += HBand.TSUQ_DCTH(Quant, Thres);
		Count += VBand.TSUQ_DCTV(Quant, Thres);
	} else {
		Count += HBand.TSUQ(Quant, Thres);
		Count += VBand.TSUQ(Quant, Thres);
	}

	if (pLow != 0) {
		Count += pLow->TSUQ<use_dct>(Quant, Thres);
	} else {
		Count += LBand.TSUQ(Quant, 0.5f);
	}
	return Count;
}

template unsigned int CWavelet2D::TSUQ<false>(short, float);
template unsigned int CWavelet2D::TSUQ<true>(short, float);

template <bool use_dct>
	void CWavelet2D::TSUQi(short Quant)
{
	DBand.TSUQi(Quant);
	if (use_dct) {
		HBand.TSUQ_DCTHi(Quant);
		VBand.TSUQ_DCTVi(Quant);
	} else {
		HBand.TSUQi(Quant);
		VBand.TSUQi(Quant);
	}

	if (pLow != 0){
		pLow->TSUQi<use_dct>(Quant);
	} else {
		LBand.TSUQi(Quant);
	}
}

template void CWavelet2D::TSUQi<false>(short Quant);
template void CWavelet2D::TSUQi<true>(short Quant);

#define PRINT_STAT(band) \
	cout << band << " :\t"; \
	/*cout << "Moyenne : " << Mean << endl;*/ \
	cout << Var << endl;

void CWavelet2D::Stats(void)
{
	float Mean = 0, Var = 0;
	DBand.Mean(Mean, Var);
	PRINT_STAT("D");
	HBand.Mean(Mean, Var);
	PRINT_STAT("H");
	VBand.Mean(Mean, Var);
	PRINT_STAT("V");

	if (pLow != 0)
		pLow->Stats();
	else{
		LBand.Mean(Mean, Var);
		PRINT_STAT("L");
	}
}

#define MUL_FASTER

inline short CWavelet2D::mult08(short a)
{
#ifdef MUL_FASTER
	return (a * 0xCCCCu) >> 16;
#else
	a -= a >> 2;
	a += a >> 4;
	return a + (a >> 8);
#endif
}

void CWavelet2D::TransLine97(short * i, int len)
{
	short * iend = i + len - 5;

	short tmp = i[0] + i[2];
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

void CWavelet2D::TransLine97I(short * i, int len)
{
	short * iend = i + len - 5;

	i[0] -= i[1] - (i[1] >> 4);

	short tmp = i[1] + i[3];
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

void CWavelet2D::Transform97(short * pImage, int Stride)
{
	short * i[6];
	i[0] = pImage;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	short * out[4] = {pImage, VBand.pBand, HBand.pBand, DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = LBand.pBand;
		out_stride[0] = LBand.DimXAlign;
	}

	for( int j = 0; j < 5; j++)
		TransLine97(i[j], DimX);

	for(int k = 0 ; k < DimX; k++) {
		short tmp = i[0][k] + i[2][k];
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
			short tmp = i[3][k] + i[5][k];
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
			short tmp = i[0][k] + i[2][k];
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
			short tmp = i[0][k] + i[2][k];
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

void CWavelet2D::Transform97I(short * pImage, int Stride)
{
	short * in[4] = {pImage, VBand.pBand, HBand.pBand, DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	short * i[6];
	i[0] = pImage - DimY * Stride;
	if (pHigh != 0) i[0] += Stride - DimX;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		short tmp = k & 1;
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

			short tmp = i[3][k] + i[5][k];
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
			short tmp = i[0][k] + i[2][k];
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
			short tmp = i[0][k] + i[2][k];
			i[1][k] += tmp + (tmp >> 1);

			i[3][k] += i[2][k] * 2 + i[2][k];
		}
	}

	for( int j = 0; j < 4; j++)
		TransLine97I(i[j], DimX);
}

void CWavelet2D::TransLine53(short * i, int len)
{
	short * iend = i + len - 3;

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

void CWavelet2D::TransLine53I(short * i, int len)
{
	short * iend = i + len - 3;

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

void CWavelet2D::Transform53(short * pImage, int Stride)
{
	short * i[4];
	i[0] = pImage;
	for( int j = 1; j < 4; j++)
		i[j] = i[j-1] + Stride;

	short * out[4] = {pImage, VBand.pBand, HBand.pBand, DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = LBand.pBand;
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

void CWavelet2D::Transform53I(short * pImage, int Stride)
{
	short * in[4] = {pImage, VBand.pBand, HBand.pBand, DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	short * i[4];
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

void CWavelet2D::TransLineHaar(short * i, int len)
{
	short * iend = i + len - 1;

	for( ; i < iend; i += 2) {
		i[1] -= i[0];
		i[0] += i[1] >> 1;
	}
}

void CWavelet2D::TransLineHaarI(short * i, int len)
{
	short * iend = i + len - 1;

	for( ; i < iend; i += 2) {
		i[0] -= i[1] >> 1;
		i[1] += i[0];
	}
}

void CWavelet2D::TransformHaar(short * pImage, int Stride)
{
	short * i[2];
	i[0] = pImage;
	i[1] = i[0] + Stride;

	short * out[4] = {pImage, VBand.pBand, HBand.pBand, DBand.pBand};
	int out_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		out[0] = LBand.pBand;
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

void CWavelet2D::TransformHaarI(short * pImage, int Stride)
{
	short * in[4] = {pImage, VBand.pBand, HBand.pBand, DBand.pBand};
	int in_stride[4] = {Stride, VBand.DimXAlign, HBand.DimXAlign, DBand.DimXAlign};
	if (pLow == 0){
		in[0] = LBand.pBand;
		in_stride[0] = LBand.DimXAlign;
	} else {
		in[0] -= (pLow->DimY - 1) * Stride + pLow->DimX;
	}

	short * i[2];
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

void CWavelet2D::Transform(short * pImage, int Stride, trans t)
{
	if (t == cdf97) {
		Transform97(pImage, Stride);
	} else if (t == cdf53) {
		Transform53(pImage, Stride);
	} else if (t == haar)
		TransformHaar(pImage, Stride);

	if (pLow != 0)
		pLow->Transform(pImage, Stride, t);
}

void CWavelet2D::TransformI(short * pImage, int Stride, trans t)
{
	if (pLow != 0)
		pLow->TransformI(pImage, Stride, t);

	if (t == cdf97) {
		Transform97I(pImage, Stride);
	} else if (t == cdf53) {
		Transform53I(pImage, Stride);
	} else if (t == haar)
		TransformHaarI(pImage, Stride);
}

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

void CWavelet2D::SetWeight(trans t, float baseWeight)
{
	float scale, scale2;
	if (t == cdf97) {
		scale = XI;
		scale2 = XI * XI;
	} else {
		scale = SQRT2;
		scale2 = 2;
	}

	if (pHigh != 0){
		DBand.Weight = pHigh->VBand.Weight;
		VBand.Weight = pHigh->LBand.Weight;
		HBand.Weight = VBand.Weight;
		LBand.Weight = VBand.Weight * scale2;
	}else{
		DBand.Weight = baseWeight / scale2;
		VBand.Weight = baseWeight;
		HBand.Weight = baseWeight;
		LBand.Weight = baseWeight * scale2;
	}

	if (pLow != 0)
		pLow->SetWeight(t);
}

}
