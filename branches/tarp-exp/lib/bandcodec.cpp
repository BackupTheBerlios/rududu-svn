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

#include <math.h>

#include "bandcodec.h"
#include "huffcodec.h"

#ifdef GENERATE_HUFF_STATS
#include <math.h>
#include <string.h>
#include <iostream>
using namespace std;
#endif

namespace rududu {

CBandCodec::CBandCodec(void):
	CBand(),
	pRD(0)
{
	init_lut();
#ifdef ERR_SH
	q_err = 0;
#endif
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
	for( unsigned int i = 0; i < sizeof(huff_0_dec) / sizeof(sHuffCan); i++){
		CHuffCodec::init_lut(&huff_0_dec[i], LUT_DEPTH);
	}
	for( unsigned int i = 0; i < sizeof(huff_X_dec) / sizeof(sHuffCan); i++){
		CHuffCodec::init_lut(&huff_X_dec[i], LUT_DEPTH);
	}
	init = true;
}

#ifdef GENERATE_HUFF_STATS
uint CBandCodec::sum_0_cnt[SUM_CTX_NB][MAX_P - 1];
uint CBandCodec::sum_x_cnt[SUM_CTX_NB][MAX_P - 3]; // 3 is for MAX_P = 21
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
			geoCodec.code(s2u(pCur[i] - pCur[i - 1]), DEPRECATED_BIT_CONTEXT_NB - 1);
		else
			pCur[i] = pCur[i - 1] + u2s(geoCodec.decode(DEPRECATED_BIT_CONTEXT_NB - 1));
	}
	pCur += stride;

	for( unsigned int j = 1; j < DimY; j++){
		if (mode == encode)
			geoCodec.code(s2u(pCur[0] - pCur[-stride]), DEPRECATED_BIT_CONTEXT_NB - 1);
		else
			pCur[0] = pCur[-stride] + u2s(geoCodec.decode(DEPRECATED_BIT_CONTEXT_NB - 1));

		for( unsigned int i = 1; i < DimX; i++){
			int var = ABS(pCur[i - 1] - pCur[i - 1 - stride]) +
					ABS(pCur[i - stride] - pCur[i - 1 - stride]);
			var = min(DEPRECATED_BIT_CONTEXT_NB - 2, bitlen(var));
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

const char len_1[BLK_SIZE * BLK_SIZE] = {
	5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 8, 10, 12, 14, 17, 20
};

template <class C>
	void CBandCodec::makeThres(C * thres, const C quant, const int lambda)
{
	for( int i = 0; i < BLK_SIZE * BLK_SIZE; i++){
		thres[i] = (quant + ((lambda * (blen[i + 1] - blen[i] + len_1[i]/*clen(1, i + 1)*/) + 8) >> 4)) & 0xFFFE;
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
	int cor = (1 << 15);
#ifdef ERR_SH
	cor -= (q_err >> ERR_SH);
	if (cor > (1 << 15))
		cor = (1 << 15);
#endif
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
					int qtmp = (tmp * iQuant + cor) >> 16;
					if (qtmp <= 0) qtmp = 1;
					int diff = qtmp * Quant;
#ifdef ERR_SH
					if (qtmp > 1)
						q_err += diff - tmp;
#endif
					if (diff > tmp) diff = 2 * tmp - diff; // FIXME c'est quoi ce calcul de distorsion bidon ?
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
			int diff = Quant - (pCur[i] & (C)0xFFFFFFFE); // FIXME : pkoi x2 ?
			dist += diff;
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
		pCur += DimXAlign * BLK_SIZE;
		pRD += rd_stride;
		pChild[0] += child_stride;
		pChild[1] += child_stride;
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

template <int block_size, class C>
	int CBandCodec::maxLen(C * pBlock, int stride)
{
	if (block_size == 1)
		return bitlen(ABS(pBlock[0]));

	C max = 0;
	for( int j = 0; j < block_size; j++) {
		for( int i = 0; i < block_size; i++)
			max = MAX(max, pBlock[i]);
		pBlock += stride;
	}
	return bitlen(U(max) >> 1);
}

/**
 * find the best k (shift factor) for Rice coding based on the sum of the
 * 16 samples of a block;
 *
 * @param sum sum of the 16 samples
 * @return k * 2 if k is the best estimate and k - 1 the second best or
 * k * 2 + 1 if k is the best estimate and k + 1 the second best
 */
static int get_best_k_estimate(int sum) {
	const int thres[] =
	{2, 9, 24, 56, 119, 247, 503, 1014, 2036, 4080, 8169, 16347, 32703, 0xFFFFFF}; // 1/2 + 1/32
		// {2, 9, 25, 57, 122, 253, 514, 1036, 2081, 4170, 8348, 16705, 33419, 0xFFFFFF}; // 1/2
	const int thres2[] = {5, 16, 38, 84, 176, 361, 730, 1469, 2946, 5901, 11810, 23629, 47266}; // 0
		// {3, 12, 31, 69, 147, 302, 613, 1234, 2476, 4960, 9929, 19867, 39743}; // 1/4

	int s = 0;
	while (thres[s] < sum)
		s++;
	if (s > 0) {
		if (thres2[s - 1] <= sum)
			s = (s << 1) + 1;
		else
			s <<= 1;
	}
	return s;
}

/**
 * find the best k (shift factor) for Rice coding for a 4x4 bloc using
 *
 * @param s samples array
 * @param k number of samples (<= 16, only samples != 0)
 * @return the best shift factor + 2 (0 is 2, 0 and 1 allows k = -2 and -1)
 */
template <class C>
static int get_best_k(C * s, int k) {
	int ibest = 0, best_len = 0xFFFF;
	int bits[2][2] = {{12, 32}, {53, 16}}; // {1.48975 * 8, 4 * 8}, {6.6406 * 8, 2 * 8}
	for (int i = 0; i < 2; i++) {
		int len = bits[i][0];
		for (uint j = 0; j < k; j++)
			len += (U(s[j]) >> 1) * bits[i][1];
		if (len <= best_len) {
			best_len = len;
			ibest = i;
		}
	}
	for (int i = 1; i < 13; i++) {
		int len = 16 * i;
		for (uint j = 0; j < k; j++)
			len += U(s[j]) >> i;
		len <<= 3;
		if (len <= best_len) {
			best_len = len;
			ibest = i + 1;
		}
	}
	return ibest;
}

#define EST_SIZE 64
#define EST_SPEED 3
#define EST_SHIFT 7
static unsigned short k_est[EST_SIZE];
// #define EST_CNT 8
// static unsigned int k_cnt[EST_CNT][EST_SIZE];

template <cmode mode, bool high_band, class C>
	unsigned int CBandCodec::block_enum(C * pBlock, int stride, CMuxCodec * pCodec,
	                                    CBitCodec<ZBLOCK_CTX_NB> & zblockCodec, int sum_)
{
	uint k = 0;
	short enu[16];
	C tmp[16];
	int idx = log2i(sum_);

	int tst = idx;
	if (tst > 0) {
		tst -= 14;
		if (tst < 1)
			tst = 1;
	}

	int est = (k_est[tst] + (1 << (EST_SHIFT - 1))) >> EST_SHIFT;

	if (mode == encode) {
		int sum = 0;

		for (int j = 0; j < 4; j++){
			for (C * pEnd = pBlock + 4; pBlock < pEnd; pBlock++){
				tmp[k++] = pBlock[0];
				sum += pBlock[0] >> 1;
			}
			pBlock += stride - 4;
		}

		if (!high_band && zblockCodec.code(sum == 0, idx >= ZBLOCK_CTX_NB ? ZBLOCK_CTX_NB - 1 : idx))
			return 0;

		k = 0;
		while (sum >= MAX_P) {
			sum = 0;
			k++;
			for (int i = 0; i < 16; i++)
				sum += tmp[i] >> (k + 1);
		}

		for (int i = 0; i < 16; i++)
			enu[i] = tmp[i] >> (k + 1);


		if (est == 0) {
			if (k_est[tst] < (1 << (EST_SHIFT - 3)))
				pCodec->golombCode(k, -3); // TODO adaptive golomb here !
			else
				pCodec->golombCode(k, -1);
		} else {
			int diff = k - est;
// 			int est2 = (k_est[tst] + (1 << (EST_SHIFT - 3))) >> (EST_SHIFT - 2);
// 			if (est2 <= EST_SIZE && k < EST_CNT)
// 				k_cnt[k][est2 - 1]++;
			if (k_est[tst] > (est << EST_SHIFT)) // TODO test if really usefull
				diff = -diff;
			pCodec->golombCode(s2u(diff), 0); // TODO and maybe here too !
		}

		if (k == 0) {
			int tmp = bitlen(sum_);
			if (unlikely(sum_ == 0)) tmp = 3;
			if (tmp > SUM_CTX_NB) tmp = SUM_CTX_NB;
			pCodec->huffCode(huff_0_enc[tmp - 1][sum - 1]);
#ifdef GENERATE_HUFF_STATS
			sum_0_cnt[tmp - 1][sum - 1]++;
#endif
		} else {
			int tmp = (sum_ - (2 << k)) >> (k + 2);
			if (tmp < 0) tmp = 0;
			if (unlikely(sum_ == 0)) tmp = 2;
			if (tmp >= SUM_CTX_NB) tmp = SUM_CTX_NB - 1;
			pCodec->huffCode(huff_X_enc[tmp][sum - 3]);
#ifdef GENERATE_HUFF_STATS
			sum_x_cnt[tmp][sum - 3]++;
#endif
		}
		pCodec->enumCode(enu, sum);
		uint mask = ((1 << k) - 1);
		for (int i = 0; i < 16; i++) {
			if (k > 0)
				pCodec->bitsCode((tmp[i] >> 1) & mask, k);
			if (tmp[i] != 0)
				pCodec->bitsCode(tmp[i] & 1, 1);
		}
	} else {
		if (!high_band && zblockCodec.decode(idx >= ZBLOCK_CTX_NB ? ZBLOCK_CTX_NB - 1 : idx))
			return 0;
		if (est == 0) {
			if (k_est[tst] < (1 << (EST_SHIFT - 3)))
				k = pCodec->golombDecode(-3);
			else
				k = pCodec->golombDecode(-1);
		} else {
			int diff = u2s(pCodec->golombDecode(0));
			if (k_est[tst] > (est << EST_SHIFT))
				diff = -diff;
			k = est + diff;
		}
		int sum;
		if (k == 0) {
			int tmp = bitlen(sum_);
			if (unlikely(sum_ == 0)) tmp = 3;
			if (tmp > SUM_CTX_NB) tmp = SUM_CTX_NB;
			sum = pCodec->huffDecode(&huff_0_dec[tmp - 1]) + 1;
		} else {
			int tmp = (sum_ - (2 << k)) >> (k + 2);
			if (tmp < 0) tmp = 0;
			if (unlikely(sum_ == 0)) tmp = 2;
			if (tmp >= SUM_CTX_NB) tmp = SUM_CTX_NB - 1;
			sum = pCodec->huffDecode(&huff_X_dec[tmp]) + 3;
		}
		pCodec->enumDecode(enu, sum);
		for (int i = 0; i < 16; i++) {
			tmp[i] = enu[i];
			if (k > 0)
				tmp[i] = (tmp[i] << k) | pCodec->bitsDecode(k);
			if (tmp[i] != 0)
				tmp[i] = (tmp[i] << 1) | pCodec->bitsDecode(1);
		}

		int i = 0;
		for (int j = 0; j < 4; j++){
			for (C * pEnd = pBlock + 4; pBlock < pEnd; pBlock++){
				pBlock[0] = tmp[i++];
			}
			pBlock += stride - 4;
		}
	}

	if (tst < EST_SIZE)
		k_est[tst] += ((k << EST_SHIFT) - k_est[tst]) >> EST_SPEED;

	return k;
}

#define TREE_CTX_NB	16

template <cmode mode, bool high_band, class C, class P>
	void CBandCodec::tree(CMuxCodec * pCodec)
{
// 	static const int thres[] = {0, 2, 4, 7, 11, 17, 26, 39, 57, 82, 119, 170, 242, 344, 489, 694, 983, 1393, 1971, 2790, 3947, 5585, 7900, 11175, 15805, 22355, 0xFFFFFF};

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

	CBitCodec<TREE_CTX_NB> treeCodec(pCodec);
	CBitCodec<ZBLOCK_CTX_NB> zblockCodec(pCodec);

	// TODO : pb rate-distortion : c'est quoi cette courbe ?
	// TODO : utiliser le prédicteur comme un predicteur (coder l'erreur) ?
	// TODO : diminuer nb de contexts pour ajouter ctx variance
	// TODO : tester joindre les ctx k et 16-k -> semble pas possible à cause 0
	// TODO : tester block à 0/16 ? => puis arbre à 0 (ou != 0 ?)

	for( uint j = 0; j <= DimY - BLK_SIZE; j += BLK_SIZE){
		int i = 0, bs = BLK_SIZE;
// 		if (j & BLK_SIZE) {
// 			bs = -bs;
// 			i = (DimX & (-1 << BLK_PWR)) + bs;
// 		}
		for( ; (uint)i <= DimX - BLK_SIZE; i += bs){
			int ctx = 15, k = i >> 1;
			if (pPar) ctx = pPar[k];

			if (ctx == INSIGNIF_BLOCK) {
				pPar[k] = 0;
				if (!high_band)
					pCur1[i] = pCur1[i + (BLK_SIZE >> 1)] = pCur2[i] = pCur2[i + (BLK_SIZE >> 1)] = INSIGNIF_BLOCK;
			} else {
				if (pPar) ctx = maxLen<(BLK_SIZE >> 1)>(&pPar[k], pParent->DimXAlign);
				if ((high_band || ctx < 2) &&
					((mode == encode && treeCodec.code(pCur1[i] == INSIGNIF_BLOCK, ctx))
					|| (mode == decode && treeCodec.decode(ctx)))) {
					if (!high_band)
						pCur1[i] = pCur1[i + (BLK_SIZE >> 1)] = pCur2[i] = pCur2[i + (BLK_SIZE >> 1)] = INSIGNIF_BLOCK;
				} else {
					int est = 0;
					if (i != 0 && j != 0) {
						// TODO regenerate huff tables with this sum :
						int sum = /*(pCur1[-DimXAlign + i - 1] >> 1) + */(pCur1[-DimXAlign + i] >> 1) + (pCur1[-DimXAlign + 1 + i] >> 1) + (pCur1[-DimXAlign + 2 + i] >> 1) + (pCur1[-DimXAlign + 3 + i] >> 1);
						sum += (pCur1[-1 + i] >> 1) + (pCur1[DimXAlign - 1 + i] >> 1) + (pCur2[-1 + i] >> 1) + (pCur2[DimXAlign - 1 + i] >> 1);
// 						while (thres[est] < sum)
// 							est++;
// 						est++;
						est = sum + 1;
					}

					pCur1[i] &= ~INSIGNIF_BLOCK;

					block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, zblockCodec, est);
				}
			}
		}
		pCur1 += diff;
		pCur2 += diff;
		pPar += diff_par;
	}
// 	if (high_band) {
// 		printf("-->\n");
// 		for (int j = 0; j < EST_SIZE; j++) {
// 			for (int i = 0; i < EST_CNT; i++)
// 				printf("%i	", k_cnt[i][j]);
// 			printf("\n");
// 		}
// 		printf("\n");
// 	}
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

static const sHuffSym h0c00[20] = { {1, 1}, {3, 3}, {2, 3}, {3, 4}, {2, 4}, {3, 5}, {5, 6}, {4, 6}, {3, 6}, {5, 7}, {4, 7}, {3, 7}, {5, 8}, {4, 8}, {3, 8}, {2, 8}, {3, 9}, {2, 9}, {1, 9}, {0, 9} };
static const sHuffSym h0d00[] = { {0x8000, 1, 1}, {0x4000, 3, 4}, {0x2000, 4, 6}, {0x1800, 5, 8}, {0xc00, 6, 11}, {0x600, 7, 14}, {0x200, 8, 17}, {0x0, 9, 19} };
static const uchar h0d_lut00[20] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };

static const sHuffSym h0c01[20] = { {3, 2}, {5, 3}, {4, 3}, {3, 3}, {5, 4}, {4, 4}, {3, 4}, {5, 5}, {4, 5}, {3, 5}, {5, 6}, {4, 6}, {3, 6}, {5, 7}, {4, 7}, {3, 7}, {2, 7}, {1, 7}, {1, 8}, {0, 8} };
static const sHuffSym h0d01[] = { {0xc000, 2, 3}, {0x6000, 3, 6}, {0x3000, 4, 9}, {0x1800, 5, 12}, {0xc00, 6, 15}, {0x200, 7, 18}, {0x0, 8, 19} };
static const uchar h0d_lut01[20] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };

static const sHuffSym h0c02[20] = { {13, 4}, {12, 4}, {7, 3}, {11, 4}, {9, 4}, {10, 4}, {8, 4}, {7, 4}, {6, 4}, {5, 4}, {4, 4}, {3, 4}, {5, 5}, {4, 5}, {3, 5}, {2, 5}, {3, 6}, {2, 6}, {1, 6}, {0, 6} };
static const sHuffSym h0d02[] = { {0xe000, 3, 7}, {0x3000, 4, 14}, {0x1000, 5, 17}, {0x0, 6, 19} };
static const uchar h0d_lut02[20] = { 2, 0, 1, 3, 5, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };

static const sHuffSym h0c03[20] = { {0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}, {4, 4}, {5, 4}, {8, 4}, {10, 4}, {13, 4}, {15, 4}, {14, 4}, {12, 4}, {11, 4}, {9, 4}, {7, 4}, {6, 4} };
static const sHuffSym h0d03[] = { {0x4000, 4, 15}, {0x0, 5, 19} };
static const uchar h0d_lut03[20] = { 13, 14, 12, 15, 16, 11, 17, 10, 18, 19, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };



static const sHuffSym hXc00[18] = { {0, 8}, {1, 8}, {1, 7}, {5, 5}, {4, 3}, {7, 3}, {6, 3}, {5, 3}, {3, 3}, {5, 4}, {4, 4}, {3, 4}, {4, 5}, {3, 5}, {2, 5}, {3, 6}, {2, 6}, {1, 6} };
static const sHuffSym hXd00[] = { {0x6000, 3, 7}, {0x3000, 4, 10}, {0x1000, 5, 13}, {0x400, 6, 15}, {0x200, 7, 16}, {0x0, 8, 17} };
static const uchar hXd_lut00[18] = { 5, 6, 7, 4, 8, 9, 10, 11, 3, 12, 13, 14, 15, 16, 17, 2, 1, 0 };

static const sHuffSym hXc01[18] = { {0, 8}, {1, 8}, {1, 7}, {2, 5}, {6, 4}, {6, 3}, {7, 3}, {5, 3}, {4, 3}, {7, 4}, {5, 4}, {4, 4}, {3, 4}, {5, 5}, {4, 5}, {3, 5}, {1, 5}, {1, 6} };
static const sHuffSym hXd01[] = { {0x8000, 3, 7}, {0x3000, 4, 11}, {0x800, 5, 14}, {0x400, 6, 15}, {0x200, 7, 16}, {0x0, 8, 17} };
static const uchar hXd_lut01[18] = { 6, 5, 7, 8, 9, 4, 10, 11, 12, 13, 14, 15, 3, 16, 17, 2, 1, 0 };

static const sHuffSym hXc02[18] = { {0, 8}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {7, 4}, {6, 3}, {7, 3}, {11, 4}, {10, 4}, {9, 4}, {8, 4}, {6, 4}, {5, 4}, {4, 4}, {3, 4}, {2, 4}, {1, 4} };
static const sHuffSym hXd02[] = { {0xc000, 3, 7}, {0x1000, 4, 13}, {0x800, 5, 14}, {0x400, 6, 15}, {0x200, 7, 16}, {0x0, 8, 17} };
static const uchar hXd_lut02[18] = { 7, 6, 8, 9, 10, 11, 5, 12, 13, 14, 15, 16, 17, 4, 3, 2, 1, 0 };

static const sHuffSym hXc03[18] = { {0, 9}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4}, {8, 4}, {9, 4}, {6, 3}, {5, 3}, {7, 3} };
static const sHuffSym hXd03[] = { {0xa000, 3, 7}, {0x1000, 4, 12}, {0x800, 5, 13}, {0x400, 6, 14}, {0x200, 7, 15}, {0x100, 8, 16}, {0x0, 9, 17} };
static const uchar hXd_lut03[18] = { 17, 15, 16, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };


sHuffSym const * const CBandCodec::huff_0_enc[4] = { h0c00, h0c01, h0c02, h0c03 };
sHuffSym const * const CBandCodec::huff_X_enc[4] = { hXc00, hXc01, hXc02, hXc03 };
sHuffCan CBandCodec::huff_0_dec[4] = { {h0d00, h0d_lut00}, {h0d01, h0d_lut01}, {h0d02, h0d_lut02}, {h0d03, h0d_lut03} };
sHuffCan CBandCodec::huff_X_dec[4] = { {hXd00, hXd_lut00}, {hXd01, hXd_lut01}, {hXd02, hXd_lut02}, {hXd03, hXd_lut03} };

}
