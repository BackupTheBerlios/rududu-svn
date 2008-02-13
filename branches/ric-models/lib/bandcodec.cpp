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

#include "bandcodec.h"

namespace rududu {

CBandCodec::CBandCodec(void):
		CBand(),
		pRD(0)
{
}

CBandCodec::~CBandCodec()
{
	delete[] pRD;
}

template <cmode mode>
		void CBandCodec::pred(CMuxCodec * pCodec)
{
	short * pCur = pBand;
	int k = 6;
	const int stride = DimXAlign;

	if (mode == encode)
		pCodec->tabooCode(s2u(pCur[0]));
	else
		pCur[0] = u2s(pCodec->tabooDecode());

	for( unsigned int i = 1; i < DimX; i++){
		if (mode == encode)
			pCodec->golombCode(s2u(pCur[i] - pCur[i - 1]), k);
		else
			pCur[i] = pCur[i - 1] + u2s(pCodec->golombDecode(k));
	}
	pCur += stride;

	for( unsigned int j = 1; j < DimY; j++){
		if (mode == encode)
			pCodec->golombCode(s2u(pCur[0] - pCur[-stride]), k);
		else
			pCur[0] = pCur[-stride] + u2s(pCodec->golombDecode(k));

		for( unsigned int i = 1; i < DimX; i++){
			int var = ABS(pCur[i - 1] - pCur[i - 1 - stride]) +
					ABS(pCur[i - stride] - pCur[i - 1 - stride]);
			var -= var >> 2;
			if (var >= 32)
				var = k;
			else
				var = log_int[var];
			if (mode == encode) {
				int pred = pCur[i] - pCur[i - 1] - pCur[i - stride] +
						pCur[i - 1 - stride];
				pCodec->golombCode(s2u(pred), var);
			} else
				pCur[i] = pCur[i - 1] + pCur[i - stride] -
						pCur[i - 1 - stride] + u2s(pCodec->golombDecode(var));
		}
		pCur += stride;
	}
}

template void CBandCodec::pred<encode>(CMuxCodec *);
template void CBandCodec::pred<decode>(CMuxCodec *);

#define INSIGNIF_BLOCK -0x8000

template <int block_size>
unsigned int CBandCodec::tsuqBlock(short * pCur, int stride, short Quant, short iQuant, short T, int lambda)
{
	unsigned int dist = 0, cnt = 0, rate = 0;
	short min = 0, max = 0;
	static const unsigned char blen[block_size * block_size + 1] = {
// 		0, 11, 17, 22, 26, 29, 31, 32, 32, 32, 31, 29, 26, 22, 17, 11, 0 // theo
// 		10, 15, 22, 27, 31, 34, 37, 39, 39, 39, 38, 37, 34, 30, 25, 19, 10 // @q=9 for all bands
		8, 16, 22, 26, 30, 32, 34, 35, 35, 35, 34, 32, 30, 26, 22, 16, 8 // enum theo
	};
	for (int j = 0; j < block_size; j++) {
		for ( int i = 0; i < block_size; i++ ) {
			if ( (unsigned short) (pCur[i] + T) <= (unsigned short) (2 * T)) {
				pCur[i] = 0;
			} else {
				cnt++;
				short tmp = pCur[i];
				dist += tmp * tmp;
				pCur[i] = (pCur[i] * (int)iQuant + (1 << 15)) >> 16;
				tmp -= pCur[i] * Quant;
				dist -= tmp * tmp;
				max = MAX(max, pCur[i]);
				min = MIN(min, pCur[i]);
				pCur[i] = s2u_(pCur[i]);
				rate += bitlen(pCur[i]);
			}
		}
		pCur += stride;
	}
	
	rate = (rate + cnt) * 2 + blen[cnt];
	
	if (dist <= lambda * rate)
		return 0;
	
	Max = MAX(Max, max);
	Min = MIN(Min, min);
	Count += cnt;
	return dist - lambda * rate;
}

template <bool high_band, int block_size>
		void CBandCodec::buildTree(const short Quant, const float Thres, const int lambda)
{
	short Q = (short) (Quant / Weight);
	if (Q == 0) Q = 1;
	short iQuant = (1 << 16) / Q;
	short T = (short) (Thres * Q);
	Min = 0, Max = 0, Count = 0;
	short * pCur = pBand;
	unsigned int * pChild[2] = {0, 0};
	unsigned int child_stride = 0;
	
	if (this->pRD == 0)
		this->pRD = new unsigned int [DimX * DimY / (block_size * block_size)];
	unsigned int * pRD = this->pRD;
	unsigned int rd_stride = DimX / block_size;

	if (! high_band) {
		pChild[0] = ((CBandCodec*)this->pChild)->pRD;
		pChild[1] = pChild[0] + this->pChild->DimX / block_size;
		child_stride = this->pChild->DimX * 2 / block_size;
	}

	for( unsigned int j = 0; j < DimY; j += block_size){
		for( unsigned int i = 0, k = 0; i < DimX; i += block_size, k++){
			unsigned int rate = 0;
			unsigned long long dist = tsuqBlock<block_size>(pCur + i, DimXAlign, Q, iQuant, T, lambda);
			if (! high_band) {
				dist += pChild[0][2*k] + pChild[0][2*k + 1] + pChild[1][2*k] + pChild[1][2*k + 1];
				rate += 4 * 2;
			}
			if (dist <= lambda * rate) {
				pCur[i] = INSIGNIF_BLOCK;
				pRD[k] = 0;
			} else
				pRD[k] = MIN(dist - lambda * rate, 0xFFFFFFFFu);
		}
		pCur += DimXAlign * block_size;
		pRD += rd_stride;
		pChild[0] += child_stride;
		pChild[1] += child_stride;
	}

	if (pParent != 0)
		((CBandCodec*)pParent)->buildTree<false, block_size>(Quant, Thres, lambda);
}

template void CBandCodec::buildTree<true, BLK_SIZE>(short, float, int);

template <int block_size, cmode mode>
	int CBandCodec::maxLen(short * pBlock, int stride)
{
	if (block_size == 1)
		return bitlen(ABS(pBlock[0]));

	short max = 0, min = 0;
	for( int j = 0; j < block_size; j++){
		for( int i = 0; i < block_size; i++){
			max = MAX(max, pBlock[i]);
			if (mode == decode)
				min = MIN(min, pBlock[i]);
		}
		pBlock += stride;
	}
	if (mode == encode)
		return bitlen(max >> 1);
	min = ABS(min);
	max = MAX(max, min);
	return bitlen(max);
}

template <cmode mode, int block_size>
	void CBandCodec::block_arith(short * pBlock, int stride, CMuxCodec * pCodec,
	                             CBitCodec & lenCodec, int max_len)
{
	for( int j = 0; j < block_size; j++){
		for( int i = 0; i < block_size; i++){
			if (mode == encode) {
				short tmp = s2u_(pBlock[i]);
				int len = bitlen(tmp >> 1), k;
				for(k = 0; k < len; k++) lenCodec.code0(k);
				if (k < max_len) lenCodec.code1(k);
				if (len != 0) pCodec->bitsCode(tmp & ((1 << len) - 1), len);
			} else {
				int len = 0;
				while( len < max_len && lenCodec.decode(len) == 0 ) len++;
				if (len != 0) pBlock[i] = u2s_(pCodec->bitsDecode(len) | (1 << len));
			}
		}
		pBlock += stride;
	}
}

template <cmode mode>
	void CBandCodec::block_enum(short * pBlock, int stride, CMuxCodec * pCodec,
	                            CGeomCodec & geoCodec, CHuffCodec & kCodec)
{
	if (mode == encode) {
		unsigned int k = 0;
		short tmp[16];
		unsigned int signif = 0;

		for( int j = 0; j < 4; j++){
			for( short * pEnd = pBlock + 4; pBlock < pEnd; pBlock++){
				signif <<= 1;
				if (pBlock[0] != 0) {
					tmp[k] = pBlock[0];
					k++;
					signif |= 1;
				}
			}
			pBlock += stride - 4;
		}

		kCodec.code(k - (kCodec.nbSym == 16), pCodec);

		if (k != 0) {
			if (k != 16)
				pCodec->enum16Code(signif, k);
			for( unsigned int i = 0; i < k; i++){
				geoCodec.code((tmp[i] >> 1) - 1,  k - 1);
				pCodec->bitsCode(tmp[i] & 1, 1);
			}
		}
	} else {
		unsigned int k = kCodec.decode(pCodec) + (kCodec.nbSym == 16);

		if (k != 0) {
			unsigned int signif = 0xFFFF;
			if (k != 16)
				signif = pCodec->enum16Decode(k);

			for( int j = 0; j < 4; j++){
				for( short * pEnd = pBlock + 4; pBlock < pEnd; pBlock++){
					if (signif & (1 << 15)) {
						pBlock[0] = u2s_(((geoCodec.decode(k - 1) + 1) << 1) | pCodec->bitsDecode(1));
					}
					signif <<= 1;
				}
				pBlock += stride - 4;
			}
		}
	}
}

template <cmode mode, int block_size>
	void CBandCodec::tree(CMuxCodec * pCodec)
{
	static const unsigned char geo_init[GEO_CONTEXT_NB] = {5, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 13};

	short * pCur1 = pBand;
	short * pCur2 = pBand + DimXAlign * (block_size >> 1);
	int diff = DimXAlign * block_size;
	short * pPar = 0;
	int diff_par = 0;

	if (pParent != 0) {
		pPar = pParent->pBand;
		diff_par = pParent->DimXAlign * (block_size >> 1);
	}

	CGeomCodec geoCodec(pCodec, geo_init);
	CBitCodec treeCodec(pCodec);
	CHuffCodec kCodec(mode, 0, 17 - (pChild == 0));

	for( unsigned int j = 0; j < DimY; j += block_size){
		for( unsigned int i = 0, k = 0; i < DimX; i += block_size, k += block_size >> 1){
			int context = 0;
			if (pPar) context = pPar[k];

			if (context == INSIGNIF_BLOCK) {
				pPar[k] = 0;
				pCur1[i] = pCur1[i + (block_size >> 1)] = pCur2[i] = pCur2[i + (block_size >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				continue;
			}

			if (pPar) context = maxLen<block_size >> 1, mode>(&pPar[k], pParent->DimXAlign);
			if (mode == encode) {
				if (pCur1[i] == INSIGNIF_BLOCK) {
					treeCodec.code1(context);
					pCur1[i] = pCur1[i + (block_size >> 1)] = pCur2[i] = pCur2[i + (block_size >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				} else {
					treeCodec.code0(context);
					block_enum<mode>(&pCur1[i], DimXAlign, pCodec, geoCodec, kCodec);
				}
			} else {
				if (treeCodec.decode(context))
					pCur1[i] = pCur1[i + (block_size >> 1)] = pCur2[i] = pCur2[i + (block_size >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				else
					block_enum<mode>(&pCur1[i], DimXAlign, pCodec, geoCodec, kCodec);
			}
		}
		pCur1 += diff;
		pCur2 += diff;
		pPar += diff_par;
	}
}

template void CBandCodec::tree<encode, BLK_SIZE>(CMuxCodec * );
template void CBandCodec::tree<decode, BLK_SIZE>(CMuxCodec * );

}
