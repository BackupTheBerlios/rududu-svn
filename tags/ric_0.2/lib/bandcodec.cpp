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
#include "huffcodec.h"

#ifdef GENERATE_HUFF_STATS
#include <iostream>
using namespace std;
#endif

namespace rududu {

CBandCodec::CBandCodec(void):
		CBand(),
		pRD(0)
{
	init_lut();
}

CBandCodec::~CBandCodec()
{
	delete[] pRD;
}

void CBandCodec::init_lut(void)
{
	static bool init = false;
	if (init)
		return;
	for( unsigned int i = 0; i < sizeof(huff_lk_dec) / sizeof(sHuffCan); i++){
		CHuffCodec::init_lut(&huff_lk_dec[i], LUT_DEPTH);
	}
	for( unsigned int i = 0; i < sizeof(huff_hk_dec) / sizeof(sHuffCan); i++){
		CHuffCodec::init_lut(&huff_hk_dec[i], LUT_DEPTH);
	}
	init = true;
}

#ifdef GENERATE_HUFF_STATS
unsigned int CBandCodec::histo_l[17][17];
unsigned int CBandCodec::histo_h[17][16];
#endif

template <cmode mode, class C>
		void CBandCodec::pred(CMuxCodec * pCodec)
{
	C * pCur = (C*) pBand;
	const int stride = DimXAlign;
	static const unsigned char geo_init[GEO_CONTEXT_NB]
		= { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 15 };
	CGeomCodec geoCodec(pCodec, geo_init);

	if (mode == encode)
		pCodec->tabooCode(s2u(pCur[0]));
	else
		pCur[0] = u2s(pCodec->tabooDecode());

	for( unsigned int i = 1; i < DimX; i++){
		if (mode == encode)
			geoCodec.code(s2u(pCur[i] - pCur[i - 1]), 15);
		else
			pCur[i] = pCur[i - 1] + u2s(geoCodec.decode(15));
	}
	pCur += stride;

	for( unsigned int j = 1; j < DimY; j++){
		if (mode == encode)
			geoCodec.code(s2u(pCur[0] - pCur[-stride]), 15);
		else
			pCur[0] = pCur[-stride] + u2s(geoCodec.decode(15));

		for( unsigned int i = 1; i < DimX; i++){
			int var = ABS(pCur[i - 1] - pCur[i - 1 - stride]) +
					ABS(pCur[i - stride] - pCur[i - 1 - stride]);
			var = bitlen(var);
			if (mode == encode) {
				int pred = pCur[i] - pCur[i - 1] - pCur[i - stride] +
						pCur[i - 1 - stride];
				geoCodec.code(s2u(pred), var);
			} else
				pCur[i] = pCur[i - 1] + pCur[i - stride] -
						pCur[i - 1 - stride] + u2s(geoCodec.decode(var));
		}
		pCur += stride;
	}
}

template void CBandCodec::pred<encode, short>(CMuxCodec *);
template void CBandCodec::pred<decode, short>(CMuxCodec *);

template void CBandCodec::pred<encode, int>(CMuxCodec *);
template void CBandCodec::pred<decode, int>(CMuxCodec *);

//FIXME : what happen with int ?
#define INSIGNIF_BLOCK -0x8000

template <class C>
	void  CBandCodec::inSort(C ** pKeys, int len)
{
	for (int i = 1; i < len; i++) {
		C * tmp = pKeys[i];
		int j = i;
		while (j > 0 && U(pKeys[j - 1][0]) < U(tmp[0])) {
			pKeys[j] = pKeys[j - 1];
			j--;
		}
		pKeys[j] = tmp;
	}
}

const char CBandCodec::blen[BLK_SIZE * BLK_SIZE + 1] = {
// 		0, 11, 17, 22, 26, 29, 31, 32, 32, 32, 31, 29, 26, 22, 17, 11, 0 // theo
// 		10, 15, 22, 27, 31, 34, 37, 39, 39, 39, 38, 37, 34, 30, 25, 19, 10 // @q=9 for all bands
	20, 40, 55, 66, 75, 81, 85, 88, 89, 88, 85, 81, 75, 66, 55, 40, 20 // enum theo
};

int CBandCodec::clen(int coef, unsigned int cnt)
{
	static const unsigned char k[] =
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2};
	static const unsigned char lps_len[] =
	{3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	static const unsigned char mps_len[] =
	{1, 1, 2, 2, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

	cnt--;
	unsigned int l = (coef - 1) >> k[cnt];
	return (k[cnt] + 1 + l * lps_len[cnt]) * 5 + mps_len[cnt];
}

template <class C>
	void CBandCodec::makeThres(C * thres, const C quant, const int lambda)
{
	for( int i = 0; i < BLK_SIZE * BLK_SIZE; i++){
		thres[i] = (quant + ((lambda * (blen[i + 1] - blen[i] + clen(1, i + 1)) + 8) >> 4)) & 0xFFFE;
		if (thres[i] > quant * 2) thres[i] = quant * 2;
		if (thres[i] < (quant & 0xFFFE)) thres[i] = quant & 0xFFFE;
	}
}

template <class C>
	int CBandCodec::tsuqBlock(C * pCur, int stride, C Quant, int iQuant,
	                          int lambda, C * rd_thres)
{
	int var_cnt = 0, cnt = 0, dist = 0, rate = 0;
	C T = Quant >> 1;
	C * var[BLK_SIZE * BLK_SIZE];
	for (int j = 0; j < BLK_SIZE; j++) {
		for ( int i = 0; i < BLK_SIZE; i++ ) {
			if (U(pCur[i] + T) <= U(2 * T)) {
				pCur[i] = 0;
			} else {
				pCur[i] = s2u_(pCur[i]);
				if (U(pCur[i]) < U(rd_thres[0]))
					var[var_cnt++] = pCur + i;
				else {
					cnt++;
					int tmp = U(pCur[i]) >> 1;
					int qtmp = (tmp * iQuant + (1 << 15)) >> 16;
					int diff = qtmp * Quant;
					if (diff > tmp) diff = 2 * tmp - diff;
					dist += diff;
					pCur[i] = (qtmp << 1) | (pCur[i] & 1);
				}
			}
		}
		pCur += stride;
	}

	if (var_cnt > 0) {
		inSort(var, var_cnt);
		int i = var_cnt - 1;
		while( i >= 0 && *var[i] < rd_thres[i + cnt])
			*var[i--] = 0;
		cnt += i + 1;
		for (; i >= 0; i--) {
			dist += (pCur[i] & (C)0xFFFFFFFE) - Quant;
			*var[i] = 2 | (*var[i] & 1);
		}
	}

	for (int j = 0; j < BLK_SIZE; j++) {
		pCur -= stride;
		for ( int i = 0; i < BLK_SIZE; i++ ) {
			if (pCur[i] != 0)
				rate += clen(pCur[i] >> 1, cnt);
		}
	}

	rate += blen[cnt] - blen[0];

	// FIXME : this part of the R/D doesn't work
// 	return dist * 16 - lambda * rate;
	return cnt;
}

template <class C>
	int CBandCodec::tsuqBlock(C * pCur, int stride, C Quant, int iQuant,
	                          int lambda, int width, int height)
{
	int cnt = 0;
	C T = (Quant + ((Quant - (Quant >> 2)) >> 1)) >> 1;
	for (int j = 0; j < height; j++) {
		for ( int i = 0; i < width; i++ ) {
			if (U(pCur[i] + T) <= U(2 * T)) {
				pCur[i] = 0;
			} else {
				pCur[i] = s2u_(pCur[i]);
				cnt++;
				int tmp = U(pCur[i]) >> 1;
				int qtmp = (tmp * iQuant + (1 << 15)) >> 16;
				pCur[i] = (qtmp << 1) | (pCur[i] & 1);
			}
		}
		pCur += stride;
	}

	return cnt;
}

template <bool high_band, class C>
	void CBandCodec::buildTree(const C Quant, const int lambda)
{
	C rd_thres[BLK_SIZE * BLK_SIZE];
	int lbda = (int) (lambda / Weight);
	C Q = (short) (Quant / Weight);
	if (Q == 0) Q = 1;
	int iQuant = (1 << 16) / Q;
	makeThres(rd_thres, Q, lbda);
	C * pCur = (C*) pBand;
	unsigned int * pChild[2] = {0, 0};
	unsigned int child_stride = 0;

	if (this->pRD == 0)
		this->pRD = new unsigned int [((DimX + BLK_SIZE - 1) / BLK_SIZE) * ((DimY + BLK_SIZE - 1) / BLK_SIZE)];
	unsigned int * pRD = this->pRD;
	unsigned int rd_stride = (DimX + BLK_SIZE - 1) / BLK_SIZE;

	if (! high_band) {
		pChild[0] = ((CBandCodec*)this->pChild)->pRD;
		pChild[1] = pChild[0] + (this->pChild->DimX + BLK_SIZE - 1) / BLK_SIZE;
		child_stride = ((this->pChild->DimX + BLK_SIZE - 1) / BLK_SIZE) * 2;
	}

	for( unsigned int j = 0, i, k; j <= DimY - BLK_SIZE; j += BLK_SIZE){
		for( i = 0, k = 0; i <= DimX - BLK_SIZE; i += BLK_SIZE, k++){
			int rate = 0;
			long long dist = tsuqBlock(pCur + i, DimXAlign, Q, iQuant, lbda, rd_thres);
			if (! high_band) {
				dist += pChild[0][2*k] + pChild[0][2*k + 1] + pChild[1][2*k] + pChild[1][2*k + 1];
				rate += 4 * 5;
			}
			if (dist <= 0 /*lambda * rate*/) {
				pCur[i] = INSIGNIF_BLOCK;
				pRD[k] = 0;
			} else
				pRD[k] = MIN(dist/* - lambda * rate*/, 0xFFFFFFFFu);
		}
		if (i < DimX) {
// 			int rate = 0;
			long long dist = tsuqBlock(pCur + i, DimXAlign, Q, iQuant, lbda, DimX - i, BLK_SIZE);
			if (dist <= 0 /*lambda * rate*/) {
				pCur[i] = INSIGNIF_BLOCK;
				pRD[k] = 0;
			} else
				pRD[k] = MIN(dist/* - lambda * rate*/, 0xFFFFFFFFu);
		}
		pCur += DimXAlign * BLK_SIZE;
		pRD += rd_stride;
		pChild[0] += child_stride;
		pChild[1] += child_stride;
	}
	if (DimY & ((1 << BLK_PWR) - 1)) {
		unsigned int i, k;
		for( i = 0, k = 0; i <= DimX - BLK_SIZE; i += BLK_SIZE, k++){
// 			int rate = 0;
			long long dist = tsuqBlock(pCur + i, DimXAlign, Q, iQuant, lbda, BLK_SIZE, DimY & ((1 << BLK_PWR) - 1));
			if (dist <= 0 /*lambda * rate*/) {
				pCur[i] = INSIGNIF_BLOCK;
				pRD[k] = 0;
			} else
				pRD[k] = MIN(dist/* - lambda * rate*/, 0xFFFFFFFFu);
		}
		if (i < DimX) {
// 			int rate = 0;
			long long dist = tsuqBlock(pCur + i, DimXAlign, Q, iQuant, lbda, DimX - i, DimY & ((1 << BLK_PWR) - 1));
			if (dist <= 0 /*lambda * rate*/) {
				pCur[i] = INSIGNIF_BLOCK;
				pRD[k] = 0;
			} else
				pRD[k] = MIN(dist/* - lambda * rate*/, 0xFFFFFFFFu);
		}
	}

	if (pParent != 0) {
		if (pParent->type == sshort)
			((CBandCodec*)pParent)->buildTree<false, short>(Quant, lambda);
		else if (pParent->type == sint)
			((CBandCodec*)pParent)->buildTree<false, int>(Quant, lambda);
	}
}

template void CBandCodec::buildTree<true, short>(short, int);
template void CBandCodec::buildTree<true, int>(int, int);

template <int block_size, cmode mode, class C>
	int CBandCodec::maxLen(C * pBlock, int stride)
{
	if (block_size == 1)
		return bitlen(ABS(pBlock[0]));

	C max = 0, min = 0;
	for( int j = 0; j < block_size; j++){
		for( int i = 0; i < block_size; i++){
			max = MAX(max, pBlock[i]);
			if (mode == decode)
				min = MIN(min, pBlock[i]);
		}
		pBlock += stride;
	}
	if (mode == encode)
		return bitlen(U(max) >> 1);
	min = ABS(min);
	max = MAX(max, min);
	return bitlen(max);
}

template <cmode mode, bool high_band, class C>
	unsigned int CBandCodec::block_enum(C * pBlock, int stride, CMuxCodec * pCodec,
	                                    CGeomCodec & geoCodec, int idx)
{
	unsigned int k = 0;
	if (mode == encode) {
		C tmp[16];
		unsigned int signif = 0;

		for( int j = 0; j < 4; j++){
			for( C * pEnd = pBlock + 4; pBlock < pEnd; pBlock++){
				signif <<= 1;
				if (pBlock[0] != 0) {
					tmp[k] = pBlock[0];
					k++;
					signif |= 1;
				}
			}
			pBlock += stride - 4;
		}

		if (high_band)
			pCodec->bitsCode(huff_hk_enc[idx][k-1].code, huff_hk_enc[idx][k-1].len);
		else
			pCodec->bitsCode(huff_lk_enc[idx][k].code, huff_lk_enc[idx][k].len);

		if (high_band || k != 0) {
			if (k != 16)
				pCodec->enumCode<16>(signif, k);
			for( unsigned int i = 0; i < k; i++){
				geoCodec.code((U(tmp[i]) >> 1) - 1,  k - 1);
				pCodec->bitsCode(tmp[i] & 1, 1);
			}
		}
	} else {
		if (high_band)
			k = huff_hk_dec[idx].sym[pCodec->huffDecode(huff_hk_dec[idx].table)] + 1;
		else
			k = huff_lk_dec[idx].sym[pCodec->huffDecode(huff_lk_dec[idx].table)];

		if (high_band || k != 0) {
			unsigned int signif = 0xFFFF;
			if (k != 16)
				signif = pCodec->enumDecode<16>(k);

			for( int j = 0; j < 4; j++){
				for( C * pEnd = pBlock + 4; pBlock < pEnd; pBlock++){
					if (signif & (1 << 15)) {
						pBlock[0] = u2s_(((geoCodec.decode(k - 1) + 1) << 1) | pCodec->bitsDecode(1));
					}
					signif <<= 1;
				}
				pBlock += stride - 4;
			}
		}
	}
	return k - (high_band == true);
}

template <cmode mode, bool high_band, class C>
	unsigned int CBandCodec::block_enum(C * pBlock, int stride, CMuxCodec * pCodec,
	                                    CGeomCodec & geoCodec, int width, int height)
{
	static const unsigned char k_conv2[9][16] =
	{
		{15},
		{7, 15},
		{4, 10, 15},
		{3, 7, 11, 15},
		{2, 4, 7, 10, 12, 15},
		{1, 3, 5, 7, 9, 11, 13, 15},
		{1, 3, 4, 6, 8, 10, 11, 13, 15},
		{0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15},
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
	};

	static const unsigned char k_conv1[16] =
	{0, 1, 2, 3, 0, 4, 0, 5, 6, 0, 0, 7, 0, 0, 0, 8};

	unsigned int k = 0;
	unsigned int cnt = width * height;
	if (mode == encode) {
		C tmp[16];
		unsigned int signif = 0;

		for( int j = 0; j < height; j++){
			for( C * pEnd = pBlock + width; pBlock < pEnd; pBlock++){
				signif <<= 1;
				if (pBlock[0] != 0) {
					tmp[k] = pBlock[0];
					k++;
					signif |= 1;
				}
			}
			pBlock += stride - width;
		}

		if (high_band)
			pCodec->maxCode(k - 1, cnt - 1);
		else
			pCodec->maxCode(k, cnt);

		if (high_band || k != 0) {
			if (k != cnt)
				pCodec->enumCode(signif, k, cnt);
			for( unsigned int i = 0; i < k; i++){
				geoCodec.code((U(tmp[i]) >> 1) - 1,  k_conv2[k_conv1[cnt]][k - 1]);
				pCodec->bitsCode(tmp[i] & 1, 1);
			}
		}
	} else {
		if (high_band)
			k = pCodec->maxDecode(cnt - 1) + 1;
		else
			k = pCodec->maxDecode(cnt);

		if (high_band || k != 0) {
			unsigned int signif = 0xFFFF;
			if (k != cnt)
				signif = pCodec->enumDecode(k, cnt);
			for( int j = 0; j < height; j++){
				for( C * pEnd = pBlock + width; pBlock < pEnd; pBlock++){
					if (signif & (1 << (cnt -1))) {
						pBlock[0] = u2s_(((geoCodec.decode(k_conv2[k_conv1[cnt]][k - 1]) + 1) << 1) | pCodec->bitsDecode(1));
					}
					signif <<= 1;
				}
				pBlock += stride - width;
			}
		}
	}
	return k - (high_band == true);
}

#define K_SHIFT	10
#define K_DECAY	3
#define K_SPEED	(K_SHIFT - K_DECAY)

template <cmode mode, bool high_band, class C, class P>
	void CBandCodec::tree(CMuxCodec * pCodec)
{
	static const unsigned char geo_init[GEO_CONTEXT_NB] = {5, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 11};
	unsigned short k_mean[16] = {2 << K_SHIFT, 3 << K_SHIFT, 4 << K_SHIFT, 5 << K_SHIFT, 8 << K_SHIFT,
			11 << K_SHIFT, 13 << K_SHIFT, 14 << K_SHIFT, 15 << K_SHIFT, 15 << K_SHIFT, 15 << K_SHIFT,
			15 << K_SHIFT, 15 << K_SHIFT, 15 << K_SHIFT, 15 << K_SHIFT, 15 << K_SHIFT};

	C * pCur1 = (C*) pBand;
	C * pCur2 = (C*) pBand + DimXAlign * (BLK_SIZE >> 1);
	int diff = DimXAlign * BLK_SIZE;
	P * pPar = 0;
	int diff_par = 0;

	if (pParent != 0) {
		pPar = (P*) pParent->pBand;
		diff_par = pParent->DimXAlign * (BLK_SIZE >> 1);
	}

	if (mode == decode) Clear(false);

	CGeomCodec geoCodec(pCodec, geo_init);
	CBitCodec treeCodec(pCodec);
	CBitCodec bordCodec(pCodec);

	unsigned int j = 0;
	for( ; j <= DimY - BLK_SIZE; j += BLK_SIZE){
		int i = 0, bs = BLK_SIZE;
		if (j & BLK_SIZE) {
			bs = -bs;
			i = DimX & (-1 << BLK_PWR);
			if ((int)DimX > i) {
				if (pPar && (i >> 1) < (int)pParent->DimX && pPar[i >> 1] == INSIGNIF_BLOCK) pPar[i >> 1] = 0;
				if ((mode == encode && bordCodec.code(pCur1[i] == INSIGNIF_BLOCK)) || (mode == decode && bordCodec.decode())) {
					if (mode == encode) pCur1[i] = 0;
				} else
					block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, geoCodec, DimX - i, 4);
			}
			i += bs;
		}
		for( ; (unsigned int)i <= DimX - BLK_SIZE; i += bs){
			int ctx = 15, k = i >> 1;
			if (pPar) ctx = pPar[k];

			if (ctx == INSIGNIF_BLOCK) {
				pPar[k] = 0;
				pCur1[i] = pCur1[i + (BLK_SIZE >> 1)] = pCur2[i] = pCur2[i + (BLK_SIZE >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				continue;
			}

			if (pPar) ctx = maxLen<BLK_SIZE >> 1, mode>(&pPar[k], pParent->DimXAlign);
			if ((mode == encode && treeCodec.code(pCur1[i] == INSIGNIF_BLOCK, ctx)) || (mode == decode && treeCodec.decode(ctx))) {
				pCur1[i] = pCur1[i + (BLK_SIZE >> 1)] = pCur2[i] = pCur2[i + (BLK_SIZE >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
			} else {
				int idx = (k_mean[ctx] + (1 << (K_SHIFT - 1))) >> K_SHIFT;
				unsigned int k = block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, geoCodec, idx);
				k_mean[ctx] += (k << K_SPEED) - (k_mean[ctx] >> K_DECAY);
#ifdef GENERATE_HUFF_STATS
				if (high_band)
					histo_h[idx][k]++;
				else
					histo_l[idx][k]++;
#endif
			}
		}
		if (i > 0 && i < (int)DimX) {
			if (pPar && (i >> 1) < (int)pParent->DimX && pPar[i >> 1] == INSIGNIF_BLOCK) pPar[i >> 1] = 0;
			if ((mode == encode && bordCodec.code(pCur1[i] == INSIGNIF_BLOCK)) || (mode == decode && bordCodec.decode())) {
				if (mode == encode) pCur1[i] = 0;
			} else
				block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, geoCodec, DimX - i, 4);
		}
		pCur1 += diff;
		pCur2 += diff;
		pPar += diff_par;
	}
	if (j < DimY) {
		int i = 0, bs = BLK_SIZE;
		if (j & BLK_SIZE) {
			bs = -bs;
			i = DimX & (-1 << BLK_PWR);
			if ((int)DimX > i) {
				if (pPar && (i >> 1) < (int)pParent->DimX && (j >> 1) < pParent->DimY && pPar[i >> 1] == INSIGNIF_BLOCK) pPar[i >> 1] = 0;
				if ((mode == encode && bordCodec.code(pCur1[i] == INSIGNIF_BLOCK)) || (mode == decode && bordCodec.decode())) {
					if (mode == encode) pCur1[i] = 0;
				} else
					block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, geoCodec, DimX - i, DimY - j);
			}
			i += bs;
		}
		for( ; (unsigned int)i <= DimX - BLK_SIZE; i += bs){
			if (pPar && (j >> 1) < pParent->DimY && pPar[i >> 1] == INSIGNIF_BLOCK) pPar[i >> 1] = 0;
			if ((mode == encode && bordCodec.code(pCur1[i] == INSIGNIF_BLOCK)) || (mode == decode && bordCodec.decode())) {
				if (mode == encode) pCur1[i] = 0;
			} else
				block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, geoCodec, 4, DimY - j);
		}
		if (i > 0 && i < (int)DimX) {
			if (pPar && (i >> 1) < (int)pParent->DimX && (j >> 1) < pParent->DimY && pPar[i >> 1] == INSIGNIF_BLOCK) pPar[i >> 1] = 0;
			if ((mode == encode && bordCodec.code(pCur1[i] == INSIGNIF_BLOCK)) || (mode == decode && bordCodec.decode())) {
				if (mode == encode) pCur1[i] = 0;
			} else
				block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, geoCodec, DimX - i, DimY - j);
		}
	}
}

template void CBandCodec::tree<encode, true, short, short>(CMuxCodec * );
template void CBandCodec::tree<encode, false, short, short>(CMuxCodec * );
template void CBandCodec::tree<decode, true, short, short>(CMuxCodec * );
template void CBandCodec::tree<decode, false, short, short>(CMuxCodec * );

template void CBandCodec::tree<encode, true, short, int>(CMuxCodec * );
template void CBandCodec::tree<encode, false, short, int>(CMuxCodec * );
template void CBandCodec::tree<decode, true, short, int>(CMuxCodec * );
template void CBandCodec::tree<decode, false, short, int>(CMuxCodec * );

template void CBandCodec::tree<encode, true, int, int>(CMuxCodec * );
template void CBandCodec::tree<encode, false, int, int>(CMuxCodec * );
template void CBandCodec::tree<decode, true, int, int>(CMuxCodec * );
template void CBandCodec::tree<decode, false, int, int>(CMuxCodec * );

// low bands (!= first) huffman coding and decoding tables
static const sHuffSym hcl00[17] = { {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {3, 7}, {2, 7}, {3, 8}, {1, 8}, {2, 8}, {2, 10}, {3, 10}, {3, 11}, {2, 11}, {1, 11}, {1, 12}, {0, 12} };
static const sHuffSym hdl00[] = { {0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 3, 3}, {0x1000, 4, 4}, {0x800, 5, 5}, {0x400, 7, 8}, {0x100, 8, 10}, {0x80, 10, 13}, {0x20, 11, 15}, {0x0, 12, 16} };
static const unsigned char hdl_lut00[17] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 8, 11, 10, 12, 13, 14, 15, 16 };

static const sHuffSym hcl01[17] = { {2, 3}, {1, 1}, {3, 3}, {1, 3}, {1, 4}, {1, 5}, {3, 7}, {2, 7}, {3, 8}, {2, 8}, {3, 9}, {2, 9}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {0, 12} };
static const sHuffSym hdl01[] = { {0x8000, 1, 1}, {0x2000, 3, 4}, {0x1000, 4, 5}, {0x800, 5, 6}, {0x400, 7, 9}, {0x200, 8, 11}, {0x80, 9, 13}, {0x40, 10, 14}, {0x20, 11, 15}, {0x0, 12, 16} };
static const unsigned char hdl_lut01[17] = { 1, 2, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

static const sHuffSym hcl02[17] = { {2, 4}, {1, 1}, {3, 3}, {2, 3}, {3, 4}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {3, 9}, {2, 9}, {3, 10}, {2, 10}, {1, 10}, {1, 11}, {1, 12}, {0, 12} };
static const sHuffSym hdl02[] = { {0x8000, 1, 1}, {0x4000, 3, 4}, {0x1000, 4, 6}, {0x800, 5, 7}, {0x400, 6, 8}, {0x200, 7, 9}, {0x100, 9, 12}, {0x40, 10, 14}, {0x20, 11, 15}, {0x0, 12, 16} };
static const unsigned char hdl_lut02[17] = { 1, 2, 3, 4, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

static const sHuffSym hcl03[17] = { {3, 5}, {3, 2}, {2, 2}, {3, 3}, {2, 3}, {3, 4}, {2, 4}, {2, 5}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {0, 12} };
static const sHuffSym hdl03[] = { {0x8000, 2, 3}, {0x4000, 3, 5}, {0x2000, 4, 7}, {0x800, 5, 9}, {0x400, 6, 10}, {0x200, 7, 11}, {0x100, 8, 12}, {0x80, 9, 13}, {0x40, 10, 14}, {0x20, 11, 15}, {0x0, 12, 16} };
static const unsigned char hdl_lut03[17] = { 1, 2, 3, 4, 5, 6, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

static const sHuffSym hcl04[17] = { {2, 5}, {4, 3}, {6, 3}, {5, 3}, {7, 3}, {3, 3}, {2, 3}, {3, 4}, {2, 4}, {3, 5}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {0, 10} };
static const sHuffSym hdl04[] = { {0x4000, 3, 7}, {0x2000, 4, 9}, {0x800, 5, 11}, {0x400, 6, 12}, {0x200, 7, 13}, {0x100, 8, 14}, {0x80, 9, 15}, {0x0, 10, 16} };
static const unsigned char hdl_lut04[17] = { 4, 2, 3, 1, 5, 6, 7, 8, 9, 0, 10, 11, 12, 13, 14, 15, 16 };

static const sHuffSym hcl05[17] = { {2, 6}, {4, 4}, {3, 3}, {5, 3}, {7, 3}, {6, 3}, {4, 3}, {5, 4}, {3, 4}, {2, 4}, {3, 5}, {2, 5}, {3, 6}, {1, 6}, {1, 7}, {1, 8}, {0, 8} };
static const sHuffSym hdl05[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x1000, 5, 12}, {0x400, 6, 14}, {0x200, 7, 15}, {0x0, 8, 16} };
static const unsigned char hdl_lut05[17] = { 4, 5, 3, 6, 2, 7, 1, 8, 9, 10, 11, 12, 0, 13, 14, 15, 16 };

static const sHuffSym hcl06[17] = { {1, 8}, {3, 5}, {3, 4}, {5, 4}, {5, 3}, {6, 3}, {7, 3}, {4, 3}, {3, 3}, {4, 4}, {2, 4}, {2, 5}, {1, 5}, {1, 6}, {1, 7}, {1, 9}, {0, 9} };
static const sHuffSym hdl06[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x800, 5, 12}, {0x400, 6, 13}, {0x200, 7, 14}, {0x100, 8, 15}, {0x0, 9, 16} };
static const unsigned char hdl_lut06[17] = { 6, 5, 4, 7, 8, 3, 9, 2, 10, 1, 11, 12, 13, 14, 0, 15, 16 };

static const sHuffSym hcl07[17] = { {1, 8}, {1, 5}, {3, 5}, {4, 4}, {6, 4}, {4, 3}, {5, 3}, {6, 3}, {7, 3}, {7, 4}, {5, 4}, {3, 4}, {2, 4}, {2, 5}, {1, 6}, {1, 7}, {0, 8} };
static const sHuffSym hdl07[] = { {0x8000, 3, 7}, {0x2000, 4, 11}, {0x800, 5, 13}, {0x400, 6, 14}, {0x200, 7, 15}, {0x0, 8, 16} };
static const unsigned char hdl_lut07[17] = { 8, 7, 6, 5, 9, 4, 10, 3, 11, 12, 2, 13, 1, 14, 15, 0, 16 };

static const sHuffSym hcl08[17] = { {0, 7}, {2, 6}, {3, 6}, {3, 5}, {3, 4}, {5, 4}, {7, 4}, {5, 3}, {7, 3}, {6, 3}, {4, 3}, {6, 4}, {4, 4}, {2, 4}, {2, 5}, {1, 6}, {1, 7} };
static const sHuffSym hdl08[] = { {0x8000, 3, 7}, {0x2000, 4, 11}, {0x1000, 5, 13}, {0x400, 6, 15}, {0x0, 7, 16} };
static const unsigned char hdl_lut08[17] = { 8, 9, 7, 10, 6, 11, 5, 12, 4, 13, 3, 14, 2, 1, 15, 16, 0 };

static const sHuffSym hcl09[17] = { {0, 8}, {1, 8}, {1, 6}, {1, 5}, {3, 5}, {3, 4}, {4, 4}, {6, 4}, {5, 3}, {7, 3}, {6, 3}, {4, 3}, {7, 4}, {5, 4}, {2, 4}, {2, 5}, {1, 7} };
static const sHuffSym hdl09[] = { {0x8000, 3, 7}, {0x2000, 4, 11}, {0x800, 5, 13}, {0x400, 6, 14}, {0x200, 7, 15}, {0x0, 8, 16} };
static const unsigned char hdl_lut09[17] = { 9, 10, 8, 11, 12, 7, 13, 6, 5, 14, 4, 15, 3, 2, 16, 1, 0 };

static const sHuffSym hcl10[17] = { {0, 8}, {1, 8}, {1, 7}, {1, 6}, {3, 6}, {2, 5}, {2, 4}, {3, 4}, {5, 4}, {4, 3}, {6, 3}, {7, 3}, {5, 3}, {3, 3}, {4, 4}, {3, 5}, {2, 6} };
static const sHuffSym hdl10[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x1000, 5, 12}, {0x400, 6, 14}, {0x200, 7, 15}, {0x0, 8, 16} };
static const unsigned char hdl_lut10[17] = { 11, 10, 12, 9, 13, 8, 14, 7, 6, 15, 5, 4, 16, 3, 2, 1, 0 };

static const sHuffSym hcl11[17] = { {0, 10}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {3, 5}, {2, 4}, {2, 3}, {4, 3}, {5, 3}, {7, 3}, {6, 3}, {3, 3}, {3, 4}, {2, 5} };
static const sHuffSym hdl11[] = { {0x4000, 3, 7}, {0x2000, 4, 9}, {0x800, 5, 11}, {0x400, 6, 12}, {0x200, 7, 13}, {0x100, 8, 14}, {0x80, 9, 15}, {0x0, 10, 16} };
static const unsigned char hdl_lut11[17] = { 12, 13, 11, 10, 14, 9, 15, 8, 7, 16, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hcl12[17] = { {0, 11}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {3, 4}, {2, 3}, {4, 3}, {5, 3}, {7, 3}, {6, 3}, {3, 3}, {2, 4} };
static const sHuffSym hdl12[] = { {0x4000, 3, 7}, {0x1000, 4, 9}, {0x800, 5, 10}, {0x400, 6, 11}, {0x200, 7, 12}, {0x100, 8, 13}, {0x80, 9, 14}, {0x40, 10, 15}, {0x0, 11, 16} };
static const unsigned char hdl_lut12[17] = { 13, 14, 12, 11, 15, 10, 9, 16, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hcl13[17] = { {0, 13}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {3, 3}, {4, 3}, {3, 2}, {5, 3}, {2, 3} };
static const sHuffSym hdl13[] = { {0xc000, 2, 3}, {0x2000, 3, 6}, {0x1000, 4, 7}, {0x800, 5, 8}, {0x400, 6, 9}, {0x200, 7, 10}, {0x100, 8, 11}, {0x80, 9, 12}, {0x40, 10, 13}, {0x20, 11, 14}, {0x10, 12, 15}, {0x0, 13, 16} };
static const unsigned char hdl_lut13[17] = { 14, 15, 13, 12, 16, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hcl14[17] = { {0, 15}, {1, 15}, {1, 14}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {2, 2}, {3, 2}, {1, 2} };
static const sHuffSym hdl14[] = { {0x4000, 2, 3}, {0x2000, 3, 4}, {0x1000, 4, 5}, {0x800, 5, 6}, {0x400, 6, 7}, {0x200, 7, 8}, {0x100, 8, 9}, {0x80, 9, 10}, {0x40, 10, 11}, {0x20, 11, 12}, {0x10, 12, 13}, {0x8, 13, 14}, {0x4, 14, 15}, {0x0, 15, 16} };
static const unsigned char hdl_lut14[17] = { 15, 14, 16, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hcl15[17] = { {1, 16}, {0, 16}, {1, 15}, {1, 14}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1} };
static const sHuffSym hdl15[] = { {0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 3, 3}, {0x1000, 4, 4}, {0x800, 5, 5}, {0x400, 6, 6}, {0x200, 7, 7}, {0x100, 8, 8}, {0x80, 9, 9}, {0x40, 10, 10}, {0x20, 11, 11}, {0x10, 12, 12}, {0x8, 13, 13}, {0x4, 14, 14}, {0x2, 15, 15}, {0x0, 16, 16} };
static const unsigned char hdl_lut15[17] = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 0, 1 };

static const sHuffSym hcl16[17] = { {0, 15}, {1, 13}, {1, 14}, {1, 15}, {1, 12}, {1, 11}, {1, 10}, {2, 10}, {3, 10}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1} };
static const sHuffSym hdl16[] = { {0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 3, 3}, {0x1000, 4, 4}, {0x800, 5, 5}, {0x400, 6, 6}, {0x200, 7, 7}, {0x100, 8, 8}, {0x40, 10, 11}, {0x20, 11, 12}, {0x10, 12, 13}, {0x8, 13, 14}, {0x4, 14, 15}, {0x0, 15, 16} };
static const unsigned char hdl_lut16[17] = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 1, 2, 3, 0 };


// high bands (== first) huffman coding and decoding tables
static const sHuffSym hch01[16] = { {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 10}, {1, 9}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {0, 15} };
static const sHuffSym hdh01[] = { {0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 3, 3}, {0x1000, 4, 4}, {0x800, 5, 5}, {0x400, 6, 6}, {0x200, 7, 7}, {0x100, 8, 8}, {0x80, 9, 9}, {0x40, 10, 10}, {0x20, 11, 11}, {0x10, 12, 12}, {0x8, 13, 13}, {0x4, 14, 14}, {0x0, 15, 15} };
static const unsigned char hdh_lut01[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 8, 10, 11, 12, 13, 14, 15 };

static const sHuffSym hch02[16] = { {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {0, 15} };
static const sHuffSym hdh02[] = { {0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 3, 3}, {0x1000, 4, 4}, {0x800, 5, 5}, {0x400, 6, 6}, {0x200, 7, 7}, {0x100, 8, 8}, {0x80, 9, 9}, {0x40, 10, 10}, {0x20, 11, 11}, {0x10, 12, 12}, {0x8, 13, 13}, {0x4, 14, 14}, {0x0, 15, 15} };
static const unsigned char hdh_lut02[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static const sHuffSym hch03[16] = { {3, 2}, {2, 2}, {3, 3}, {2, 3}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {0, 13} };
static const sHuffSym hdh03[] = { {0x8000, 2, 3}, {0x2000, 3, 5}, {0x1000, 4, 6}, {0x800, 5, 7}, {0x400, 6, 8}, {0x200, 7, 9}, {0x100, 8, 10}, {0x80, 9, 11}, {0x40, 10, 12}, {0x20, 11, 13}, {0x10, 12, 14}, {0x0, 13, 15} };
static const unsigned char hdh_lut03[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static const sHuffSym hch04[16] = { {3, 3}, {5, 3}, {4, 3}, {3, 2}, {2, 3}, {3, 4}, {2, 4}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {0, 11} };
static const sHuffSym hdh04[] = { {0xc000, 2, 3}, {0x4000, 3, 6}, {0x1000, 4, 8}, {0x800, 5, 9}, {0x400, 6, 10}, {0x200, 7, 11}, {0x100, 8, 12}, {0x80, 9, 13}, {0x40, 10, 14}, {0x0, 11, 15} };
static const unsigned char hdh_lut04[16] = { 3, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static const sHuffSym hch05[16] = { {2, 3}, {4, 3}, {6, 3}, {7, 3}, {5, 3}, {3, 3}, {3, 4}, {2, 4}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {0, 10} };
static const sHuffSym hdh05[] = { {0x4000, 3, 7}, {0x1000, 4, 9}, {0x800, 5, 10}, {0x400, 6, 11}, {0x200, 7, 12}, {0x100, 8, 13}, {0x80, 9, 14}, {0x0, 10, 15} };
static const unsigned char hdh_lut05[16] = { 3, 2, 4, 1, 5, 0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static const sHuffSym hch06[16] = { {2, 4}, {4, 4}, {5, 4}, {5, 3}, {6, 3}, {7, 3}, {4, 3}, {3, 3}, {3, 4}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {0, 9} };
static const sHuffSym hdh06[] = { {0x6000, 3, 7}, {0x1000, 4, 10}, {0x800, 5, 11}, {0x400, 6, 12}, {0x200, 7, 13}, {0x100, 8, 14}, {0x0, 9, 15} };
static const unsigned char hdh_lut06[16] = { 5, 4, 3, 6, 7, 2, 1, 8, 0, 9, 10, 11, 12, 13, 14, 15 };

static const sHuffSym hch07[16] = { {1, 5}, {3, 5}, {3, 4}, {5, 4}, {4, 3}, {6, 3}, {7, 3}, {5, 3}, {3, 3}, {4, 4}, {2, 4}, {2, 5}, {1, 6}, {1, 7}, {1, 8}, {0, 8} };
static const sHuffSym hdh07[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x800, 5, 12}, {0x400, 6, 13}, {0x200, 7, 14}, {0x0, 8, 15} };
static const unsigned char hdh_lut07[16] = { 6, 5, 7, 4, 8, 3, 9, 2, 10, 1, 11, 0, 12, 13, 14, 15 };

static const sHuffSym hch08[16] = { {1, 6}, {3, 6}, {3, 5}, {3, 4}, {4, 4}, {3, 3}, {5, 3}, {7, 3}, {6, 3}, {4, 3}, {5, 4}, {2, 4}, {2, 5}, {2, 6}, {1, 7}, {0, 7} };
static const sHuffSym hdh08[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x1000, 5, 12}, {0x400, 6, 14}, {0x0, 7, 15} };
static const unsigned char hdh_lut08[16] = { 7, 8, 6, 9, 5, 10, 4, 3, 11, 2, 12, 1, 13, 0, 14, 15 };

static const sHuffSym hch09[16] = { {1, 7}, {1, 6}, {3, 6}, {3, 5}, {2, 4}, {4, 4}, {3, 3}, {5, 3}, {7, 3}, {6, 3}, {4, 3}, {5, 4}, {3, 4}, {2, 5}, {2, 6}, {0, 7} };
static const sHuffSym hdh09[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x1000, 5, 12}, {0x400, 6, 14}, {0x0, 7, 15} };
static const unsigned char hdh_lut09[16] = { 8, 9, 7, 10, 6, 11, 5, 12, 4, 3, 13, 2, 14, 1, 0, 15 };

static const sHuffSym hch10[16] = { {0, 7}, {1, 7}, {2, 6}, {3, 6}, {3, 5}, {2, 4}, {4, 4}, {3, 3}, {4, 3}, {6, 3}, {7, 3}, {5, 3}, {5, 4}, {3, 4}, {2, 5}, {1, 6} };
static const sHuffSym hdh10[] = { {0x6000, 3, 7}, {0x2000, 4, 10}, {0x1000, 5, 12}, {0x400, 6, 14}, {0x0, 7, 15} };
static const unsigned char hdh_lut10[16] = { 10, 9, 11, 8, 7, 12, 6, 13, 5, 4, 14, 3, 2, 15, 1, 0 };

static const sHuffSym hch11[16] = { {0, 9}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {2, 5}, {3, 5}, {3, 4}, {2, 3}, {4, 3}, {6, 3}, {7, 3}, {5, 3}, {3, 3}, {2, 4}, {1, 5} };
static const sHuffSym hdh11[] = { {0x4000, 3, 7}, {0x2000, 4, 9}, {0x800, 5, 11}, {0x400, 6, 12}, {0x200, 7, 13}, {0x100, 8, 14}, {0x0, 9, 15} };
static const unsigned char hdh_lut11[16] = { 11, 10, 12, 9, 13, 8, 7, 14, 6, 5, 15, 4, 3, 2, 1, 0 };

static const sHuffSym hch12[16] = { {0, 10}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {3, 4}, {2, 3}, {4, 3}, {6, 3}, {7, 3}, {5, 3}, {3, 3}, {2, 4} };
static const sHuffSym hdh12[] = { {0x4000, 3, 7}, {0x1000, 4, 9}, {0x800, 5, 10}, {0x400, 6, 11}, {0x200, 7, 12}, {0x100, 8, 13}, {0x80, 9, 14}, {0x0, 10, 15} };
static const unsigned char hdh_lut12[16] = { 12, 11, 13, 10, 14, 9, 8, 15, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hch13[16] = { {0, 12}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {3, 4}, {2, 3}, {2, 2}, {3, 2}, {3, 3}, {2, 4} };
static const sHuffSym hdh13[] = { {0x8000, 2, 3}, {0x4000, 3, 5}, {0x1000, 4, 7}, {0x800, 5, 8}, {0x400, 6, 9}, {0x200, 7, 10}, {0x100, 8, 11}, {0x80, 9, 12}, {0x40, 10, 13}, {0x20, 11, 14}, {0x0, 12, 15} };
static const unsigned char hdh_lut13[16] = { 13, 12, 14, 11, 10, 15, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hch14[16] = { {0, 13}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {2, 3}, {2, 2}, {3, 2}, {3, 3} };
static const sHuffSym hdh14[] = { {0x8000, 2, 3}, {0x2000, 3, 5}, {0x1000, 4, 6}, {0x800, 5, 7}, {0x400, 6, 8}, {0x200, 7, 9}, {0x100, 8, 10}, {0x80, 9, 11}, {0x40, 10, 12}, {0x20, 11, 13}, {0x10, 12, 14}, {0x0, 13, 15} };
static const unsigned char hdh_lut14[16] = { 14, 13, 15, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hch15[16] = { {0, 14}, {1, 14}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {3, 2}, {2, 2} };
static const sHuffSym hdh15[] = { {0x4000, 2, 3}, {0x2000, 3, 4}, {0x1000, 4, 5}, {0x800, 5, 6}, {0x400, 6, 7}, {0x200, 7, 8}, {0x100, 8, 9}, {0x80, 9, 10}, {0x40, 10, 11}, {0x20, 11, 12}, {0x10, 12, 13}, {0x8, 13, 14}, {0x0, 14, 15} };
static const unsigned char hdh_lut15[16] = { 14, 15, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const sHuffSym hch16[16] = { {3, 12}, {2, 12}, {1, 12}, {0, 12}, {1, 9}, {3, 9}, {2, 9}, {1, 10}, {3, 8}, {2, 8}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1} };
static const sHuffSym hdh16[] = { {0x8000, 1, 1}, {0x4000, 2, 2}, {0x2000, 3, 3}, {0x1000, 4, 4}, {0x800, 5, 5}, {0x400, 6, 6}, {0x200, 8, 9}, {0x80, 9, 11}, {0x40, 10, 12}, {0x0, 12, 15} };
static const unsigned char hdh_lut16[16] = { 15, 14, 13, 12, 11, 10, 8, 9, 5, 6, 4, 7, 0, 1, 2, 3 };


sHuffSym const * const CBandCodec::huff_lk_enc[17] = { hcl00, hcl01, hcl02, hcl03, hcl04, hcl05, hcl06, hcl07, hcl08, hcl09, hcl10, hcl11, hcl12, hcl13, hcl14, hcl15, hcl16 };
sHuffSym const * const CBandCodec::huff_hk_enc[16] = { hch01, hch02, hch03, hch04, hch05, hch06, hch07, hch08, hch09, hch10, hch11, hch12, hch13, hch14, hch15, hch16 };
sHuffCan CBandCodec::huff_lk_dec[17] = { {hdl00, hdl_lut00}, {hdl01, hdl_lut01}, {hdl02, hdl_lut02}, {hdl03, hdl_lut03}, {hdl04, hdl_lut04}, {hdl05, hdl_lut05}, {hdl06, hdl_lut06}, {hdl07, hdl_lut07}, {hdl08, hdl_lut08}, {hdl09, hdl_lut09}, {hdl10, hdl_lut10}, {hdl11, hdl_lut11}, {hdl12, hdl_lut12}, {hdl13, hdl_lut13}, {hdl14, hdl_lut14}, {hdl15, hdl_lut15}, {hdl16, hdl_lut16} };
sHuffCan CBandCodec::huff_hk_dec[16] = { {hdh01, hdh_lut01}, {hdh02, hdh_lut02}, {hdh03, hdh_lut03}, {hdh04, hdh_lut04}, {hdh05, hdh_lut05}, {hdh06, hdh_lut06}, {hdh07, hdh_lut07}, {hdh08, hdh_lut08}, {hdh09, hdh_lut09}, {hdh10, hdh_lut10}, {hdh11, hdh_lut11}, {hdh12, hdh_lut12}, {hdh13, hdh_lut13}, {hdh14, hdh_lut14}, {hdh15, hdh_lut15}, {hdh16, hdh_lut16} };

}
